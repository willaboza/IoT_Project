/**
 *        Author: William Bozarth
 *
 *    Created on: February 06, 2020
 *
 * Project Title: CSE4342 Spring 2020 Project
 *
 *     File Name: main.c
 *
 *     GPIO PINS: PA2 on micro-controller used for GPIO and wired to DE pin on
 *                RS-485 chip.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "terminal.h"
#include "uart.h"
#include "gpio.h"
#include "dhcp.h"
#include "ethernet.h"
#include "timers.h"
#include "tcp.h"
#include "reboot.h"
#include "wait.h"
#include "mqtt.h"

// Pins
#define RED_LED PORTF,     1
#define BLUE_LED PORTF,    2
#define GREEN_LED PORTF,   3
#define PUSH_BUTTON PORTF, 4

// Function to Initialize Hardware
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, sysdivider of 5, creating system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}

int main(void)
{
    // Initialize Hardware
    initHw();
    initUart0();
    initEeprom();
    initTimer();
    initWatchdog();

    // Declare Variables
    USER_DATA userInput;
    uint8_t   data[MAX_PACKET_SIZE];

    // Setup UART0 Baud Rate
    setUart0BaudRate(115200, 40e6);

    putsUart0("\r\n");
    readDeviceConfig();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    // Set Variables for User Input to Initial Condition
    resetUserInput(&userInput);

    // Display Main Menu
    printMainMenu();

    while(true)
    {
        // If User Input detected, then process input
        if(kbhitUart0())
        {
            // Get User Input
            getsUart0(&userInput);

            // Tokenize User Input
            parseFields(&userInput);
        }

        if(dhcpEnabled)
        {
            // Handle renew lease request
            if(renewRequest)
            {
                sendDhcpRequestMessage(data);  // Send DHCPREQUEST
                renewRequest = false;
            }
            // Handle rebind request. On power-up rebindRequest = true
            if(rebindRequest)
            {
                if(dhcpEnabled)
                {
                    sendDhcpMessage(data, 1); // Send DHCPDISCOVER
                    rebindRequest = false;
                }
            }
            // Handle release request
            if(releaseRequest)
            {
                sendDhcpReleaseMessage(data);
                setStaticNetworkAddresses();
                etherEnableDhcpMode();
                releaseRequest = renewRequest = false;
                rebindRequest = true; // Send DHCPDISOVER Message
            }
        }
        else if(rebindRequest) // Send DHCPINFORM Message
        {
            sendDhcpInformMessage(data);
            rebindRequest = false;
        }

        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);

             // Handle IP datagram
             if (etherIsIp(data))
             {
                 if (etherIsIpUnicast(data))
                 {
                     if(etherIsTcp(data))
                     {
                         // Handle TCP messages here
                         // We are coding up at TCP Server for HTTP or Telnet
                         uint8_t type;

                         type = etherIsTcpMsgType(data);

                         if(listenState)
                         {
                             if(type == 1) // Response to Initial SYN
                             {
                                 sendTcpMessage(data, 0x6012); // Tx SYN+ACK
                             }
                             else if(type == 2)
                             {
                                 listenState = false;
                                 establishedState = true;
                                 //putsUart0("  TCP Connection Established.\r\n");
                             }
                         }
                         else if(establishedState)
                         {
                             if(type == 3) // PSH+ACK Rx
                             {
                                 getTcpData(data);
                                 sendTcpMessage(data, 0x5010); // Tx ACK
                             }
                         }
                         else if(closeState)
                         {
                             if(type == 2)
                             {
                                 closeState = false;
                                 listenState = true;
                             }
                             else if(type == 4) // Connection Termination
                             {
                                 sendTcpMessage(data, 0x5011); // Send FIN+ACK
                             }
                         }
                     }
                     else if(etherIsDhcp(data)) // Handles Unicast DHCP Messages
                     {
                         uint8_t type;

                         type = dhcpOfferType(data);

                         if(type == 2) // DHCPACK Rx
                         {
                             getDhcpAckInfo(data); // Get Information in DHCPACK Message
                         }
                         else if(type == 3) // DHCPNACK Rx
                         {
                             rebindRequest = true; // If DHCPNACK Rx then Tx DHCPDISCOVER message
                             dhcpRequestType = 0;
                         }
                     }
                     else if (etherIsPingRequest(data)) // Handle ICMP ping request
                     {
                       etherSendPingResponse(data);
                     }
                 }
                 else if(etherIsDhcp(data)) // Handles broadcast DHCP Messages
                 {
                     uint8_t type;

                     type = dhcpOfferType(data);

                     if(type == 1) // DHCPOFFER Rx
                     {
                         sendDhcpRequestMessage(data);  // Send DHCPREQUEST
                         rebindRequest = releaseRequest = false;
                     }
                     else if(type == 2) // DHCPACK Rx
                     {
                         getDhcpAckInfo(data); // Get Information in DHCPACK Message

                         // A gratuitous ARP request has the source and destination
                         // IP set to the IP of the machine issuing the packet and
                         // destination MAC is the broadcast address ff:ff:ff:ff:ff:ff
                         etherSendArpRequest(data);
                         arpResponseRx = true;
                         startOneShotTimer(arpResponseTimer, 2);
                     }
                     else if(type == 3) // DHCPNACK Rx
                     {
                         rebindRequest = true; // If DHCPNACK Rx then Tx DHCPDISCOVER message
                         dhcpRequestType = 0;
                     }
                 }
             }

             // Handle ARP request or response
             if (etherIsArpRequest(data))
             {
                 etherSendArpResponse(data);
             }
             else if(etherIsArpResponse(data))
             {
                 if(arpResponseRx)
                 {
                     // If ARP Response received before 2 second timer elapses
                     // then send decline message, invalidate IP and use static IP,
                     // wait at least 10 seconds and send another DHCP discover message.
                     sendDhcpDeclineMessage(data);
                     setStaticNetworkAddresses();
                     etherEnableDhcpMode();            // Re-enable DHCP mode
                     startOneShotTimer(waitTimer, 10); // Set 10 second "Wait" Timer before sending another DHCPDISCOVER Message
                 }
             }
        }

        if(userInput.endOfString && isCommand(&userInput, "dhcp", 2))
        {
            char *token;

            token = getFieldString(&userInput, 1); // Retrieve DHCP command from user

            if(strcmp(token, "on") == 0)           // Enables DHCP mode and stores the mode persistently in EEPROM
            {
                if(!dhcpEnabled)
                {
                    etherEnableDhcpMode();
                    writeEeprom(0x0010, (uint32_t)dhcpEnabled);
                    releaseRequest = true;
                }
            }
            else if(strcmp(token, "off") == 0) // Disables DHCP mode and stores the mode persistently in EEPROM
            {
                if(dhcpEnabled)
                {
                    resetAllTimers();                // Turn off all clocks
                    writeEeprom(0x0010, 0xFFFFFFFF); // Erase DHCP Mode in EEPROM
                    sendDhcpReleaseMessage(data);
                    setStaticNetworkAddresses();     // Update ifconfig
                    etherDisableDhcpMode();
                    sendDhcpInformMessage(data);
                }
            }
            else if(strcmp(token, "refresh") == 0) // Refresh Current IP address (if in DHCP mode)
            {
                if(dhcpEnabled)
                {
                    renewRequest = true;
                    rebindRequest = releaseRequest = false;
                    dhcpRequestType = 2;
                }
            }
            else if(strcmp(token, "release") == 0) // Release Current IP address (if in DHCP mode)
            {
                if(dhcpEnabled)
                {
                    resetAllTimers();
                    releaseRequest = true;
                    renewRequest = rebindRequest = false;
                    dhcpRequestType = 0;
                }
            }
            resetUserInput(&userInput);
        }
        else if(!dhcpEnabled && userInput.endOfString && isCommand(&userInput, "set", 6))
        {
            char token[MAX_CHARS + 1];
            uint8_t add1, add2, add3, add4;

            // Retrieve network configuration parameter
            strcpy(token,getFieldString(&userInput, 1));

            // Get Network Address
            add1 = getFieldInteger(&userInput, 2);
            add2 = getFieldInteger(&userInput, 3);
            add3 = getFieldInteger(&userInput, 4);
            add4 = getFieldInteger(&userInput, 5);

            if(strcmp(token, "ip") == 0)      // Set Internet Protocol address
            {
                etherSetIpAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0011);
            }
            else if(strcmp(token, "gw") == 0) // Set Gateway address
            {
                etherSetIpGatewayAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0012);
            }
            else if(strcmp(token, "dns") == 0) // Set Domain Name System address
            {
                setDnsAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0013);
            }
            else if(strcmp(token, "sn") == 0) // Set Sub-net Mask
            {
                etherSetIpSubnetMask(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0014);
            }
            else if(strcmp(token, "MQTT") == 0) // Set Sub-net Mask
            {
                setMqttAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0015);
            }
            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "ifconfig", 1))
        {
            userInput.fieldCount = 0;
            displayIfconfigInfo(); // displays current MAC, IP, GW, SN, DNS, and DHCP mode
            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "set", 6))
        {
            char token[MAX_CHARS + 1];
            uint8_t add1, add2, add3, add4;

            // Retrieve network configuration parameter
            strcpy(token,getFieldString(&userInput, 1));

            // Get Network Address
            add1 = getFieldInteger(&userInput, 2);
            add2 = getFieldInteger(&userInput, 3);
            add3 = getFieldInteger(&userInput, 4);
            add4 = getFieldInteger(&userInput, 5);
        }
        else if(userInput.endOfString && isCommand(&userInput, "publish", 3))
        {

        }
        else if(userInput.endOfString && isCommand(&userInput, "subscribe", 2))
        {

        }
        else if(userInput.endOfString && isCommand(&userInput, "unsubscribe", 2))
        {

        }
        else if(userInput.endOfString && isCommand(&userInput, "connect", 1))
        {

        }
        else if(userInput.endOfString && isCommand(&userInput, "disconnect", 1))
        {

        }
        else if(userInput.endOfString && isCommand(&userInput, "reboot", 1))
        {
            while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING); // Ensure no Read or Writes to EEPROM are occuring
            putsUart0("Rebooting System...\r\n");
            rebootFlag = true;
            resetUserInput(&userInput);
        }
        else if(userInput.endOfString)
        {
            resetUserInput(&userInput);
        }
    }

    return 0;
}
