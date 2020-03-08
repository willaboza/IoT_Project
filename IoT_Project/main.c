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
#include "reboot.h"
#include "wait.h"

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
//    initTimer();
//    initWatchdog();

    // Declare Variables
    USER_DATA userInput;
    uint8_t   *udpData;
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
                sendDhcpRequestMessage(data);

                renewRequest = false;
            }
            // Handle rebind request. On power-up rebindRequest = true
            if(rebindRequest)
            {
                uint8_t dhcpOption[4] = {53,1,1,255};

                sendDhcpDiscoverMessage(data, dhcpOption);

                rebindRequest = false;
            }
            // Handle release request
            if(releaseRequest)
            {
                renewRequest = false;
            }
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

             // Handle ARP request
             if (etherIsArpRequest(data))
             {
                 etherSendArpResponse(data);
             }

             // Handle IP datagram
             if (etherIsIp(data))
             {
                 if (etherIsIpUnicast(data))
                 {
                     // Handle ICMP ping request
                     if (etherIsPingRequest(data))
                     {
                       etherSendPingResponse(data);
                     }

                     // Process UDP Datagram
                     // Test this with a udp send utility like sendip
                     // If sender IP (-is) is 192.168.1.198, this will attempt to
                     // Send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
                     // sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "on" 192.168.1.199
                     // sudo sendip -p ipv4 -is 192.168.1.198 -p udp -ud 1024 -d "off" 192.168.1.199
                     if (etherIsUdp(data))
                     {
                         udpData = etherGetUdpData(data);
                         if (strcmp((char*)udpData, "on") == 0)
                             setPinValue(GREEN_LED, 1);
                         if (strcmp((char*)udpData, "off") == 0)
                             setPinValue(GREEN_LED, 0);
                         etherSendUdpResponse(data, (uint8_t*)"Received", 9);
                     }
                 }
                 else if(etherIsDhcp(data))
                 {
                     uint8_t type;
                     type = dhcpOfferType(data);
                     switch(type)
                     {
                         case 1:
                             sendDhcpRequestMessage(data);
                             break;
                         case 2:
                             setDhcpAckInfo(data);
                             // need to add code here to start renew and rebind timers
                             break;
                         default:
                             putsUart0("DHCP Request Message not Recognized.\r\n");
                     }
                 }
             }
        }

        if(userInput.endOfString && isCommand(&userInput, "dhcp", 2))
        {
            char *token;

            token = getFieldString(&userInput, 1); // Retrieve DHCP command from user

            if(strcmp(token, "on") == 0)           // Enables DHCP mode and stores the mode persistently in EEPROM
            {
                etherEnableDhcpMode();
            }
            else if(strcmp(token, "off") == 0)     // Disables DHCP mode and stores the mode persistently in EEPROM
            {
                etherDisableDhcpMode();
            }
            else if(strcmp(token, "refresh") == 0) // Refresh Current IP address (if in DHCP mode)
            {
                putsUart0("dhcp REFRESH Function.\r\n");
            }
            else if(strcmp(token, "release") == 0) // Release Current IP address (if in DHCP mode)
            {
                putsUart0("dhcp RELEASE Function.\r\n");
            }

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "set", 3))
        {
            char    token[MAX_CHARS + 1];
            char    address[MAX_CHARS + 1];

            strcpy(token,getFieldString(&userInput, 1));   // Retrieve network configuration parameter

            strcpy(address,getFieldString(&userInput, 2)); // Retrieve network parameter address

            if(strcmp(token, "ip") == 0)                   // Set Internet Protocol address
            {
                putsUart0("set IP Function.\r\n");
            }
            else if(strcmp(token, "gw") == 0)        // Set Gateway address
            {
                putsUart0("set GW Function.\r\n");
            }
            else if(strcmp(token, "dns") == 0)       // Set Domain Name System address
            {
                putsUart0("set DNS Function.\r\n");
            }
            else if(strcmp(token, "sn") == 0)        // Set Sub-net Mask
            {
                putsUart0("set SN Function.\r\n");
            }

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "ifconfig", 1))
        {
            userInput.fieldCount = 0;

            displayIfconfigInfo(); // displays current MAC, IP, GW, SN, DNS, and DHCP mode

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "reboot", 1))
        {
            putsUart0("Rebooting System...\r\n");
            rebootFlag = true;
        }
        else if(userInput.endOfString)
        {
            resetUserInput(&userInput);
        }
    }

    return 0;
}
