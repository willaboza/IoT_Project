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
#include "rtc.h"
#include "adc.h"
#include "pwm0.h"

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
    initAdc();
//    initRtc();
    initWatchdog();

    // Declare Variables
    userData userInput = {0};
    userData topic = {0};
    userData payload = {0};

    uint8_t data[MAX_PACKET_SIZE];

    // Setup UART0 Baud Rate
    setUart0BaudRate(115200, 40e6);

    sendUart0String("\r\n");
    readDeviceConfig();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(1000000);
    setPinValue(GREEN_LED, 0);

    // Set Variables for User Input to Initial Condition
    resetUserInput(&userInput); // Used for CLI
    resetUserInput(&topic);     // Used for MQTT Topics
    resetUserInput(&payload);   // Used for MQTT Payloads

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
                dhcpIpLeased = false;
                rebindRequest = true; // Send DHCPDISOVER Message
            }
        }
        else if(rebindRequest) // Send DHCPINFORM Message
        {
            sendDhcpInformMessage(data);
            rebindRequest = false;
        }


        /* Handle Sending of MQTT Packets Here */
        if(sendMqttPing) // Send Ping Request
        {
            mqttPingRequest(data, 0x5018);
            sendMqttPing = false;
            startOneShotTimer(mqttPing, (KEEP_ALIVE_TIME - 5));
        }
        else if(sendMqttConnect && dhcpEnabled && dhcpIpLeased) // Wait until DHCP finished before sending connect msg
        {
            sendTcpSyn(data, 0x6002, 1883);
            sendMqttConnect = false;
        }
        else if(sendMqttConnect && !(dhcpEnabled)) // If DHCP Mode not enabled can send connect msg right away
        {
            sendTcpSyn(data, 0x6002, 1883);
            sendMqttConnect = false;
        }
        else if(sendPubackFlag) // Send MQTT PUBACK if QoS = 1 for Rx'd PUBLISH packet
        {
            mqttPubAckRec(data, 0x5018, 4);
            sendPubackFlag = false;
        }
        else if(sendPubrecFlag) // Send MQTT PUBREC if QoS = 2 for Rx'd PUBLISH packet
        {
            mqttPubAckRec(data, 0x5018, 5);
            sendPubrecFlag = false;
        }

        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 1);
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

                         // Check if MQTT packet was received, if so process it.
                         if(mqttMessage(data))
                         {
                             processMqttMessage(data, &topic, &payload);
                             parseMqttPacket(&topic);
                             parseMqttPacket(&payload);

                             // Flag for when MQTT PUBLISH Recieved
                             pubMessageReceived = true;
                         }

                         type = etherIsTcpMsgType(data);

                         if(listenState)
                         {
                             if(type == 1) // Response to Initial SYN
                             {
                                 sendTcpMessage(data, 0x6012); // Tx SYN+ACK
                             }
                             else if(type == 6) // Tx ACK if SYN+ACK Rx'd
                             {
                                 sendTcpMessage(data, 0x5010); // Tx ACK
                                 mqttConnectMessage(data, 0x5018);       // MQTT Connect
                                 listenState = false;
                                 establishedState = true;
                             }
                             else if(type == 2) // Transition to established state
                             {
                                 listenState = false;
                                 establishedState = true;
                             }
                         }
                         else if(establishedState) // Enter TCP Established State
                         {
                             if(type == 3) // PSH+ACK Rx
                             {
                                 sendTcpMessage(data, 0x5010); // Tx ACK
                                 startOneShotTimer(mqttPing, (KEEP_ALIVE_TIME - 5)); // Keep Alive Timer for Ping Request
                             }
                             else if(type == 2) // ACK Rx
                             {
                                 tcpAckReceived(data);
                             }
                         }
                         else if(closeState) // Enter TCP Close State
                         {
                             if(type == 2)
                             {
                                 stopTimer(mqttPing); // Stop MQTT PING Timer
                                 closeState = false;
                                 listenState = true;
                             }
                             else if(type == 4) // Connection Termination
                             {
                                 stopTimer(mqttPing); // Stop MQTT PING Timer
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

        /* Start of IFTTT Rules Table */
        if(pubMessageReceived && isMqttCommand(&topic, "env", 0))
        {
            if(isMqttCommand(&topic, "temp", 1))
            {
                char str[50];
                char buffer[10];
                uint16_t temp;

                temp = getFieldInteger(&payload, 2);

                // Send Published Temperature to UART
                sprintf(str, "Degrees Celsius : %u", temp);
                sendUart0String(str);
                sendUart0String("\r\n");

                // Update env/temp with more recent value and then MQTT Publish Packet
                // sprintf(str, "%s", "env/temp");
                // sprintf(buffer, "%u", instantTemp());
                // mqttPublish(data, 0x5018, str, buffer);
            }

            if(isMqttCommand(&topic, "led", 1))
            {
                if(isMqttCommand(&topic, "green", 2))
                {
                    if(isMqttCommand(&payload, "on", 0))
                    {
                        setPinValue(GREEN_LED, 1); // Green LED ON
                        sendUart0String("  GREEN LED ON\r\n");
                    }
                    else if(isMqttCommand(&payload, "off", 0))
                    {
                        setPinValue(GREEN_LED, 0); // Green LED OFF
                        sendUart0String("  GREEN LED OFF\r\n");
                    }
                }
                else if(isMqttCommand(&topic, "red", 2))
                {
                    if(isMqttCommand(&payload, "on", 0))
                    {
                        setPinValue(RED_LED, 1); // RedLED ON
                        sendUart0String("  RED LED ON\r\n");
                    }
                    else if(isMqttCommand(&payload, "off", 0))
                    {
                        setPinValue(RED_LED, 0); // RED LED OFF
                        sendUart0String("  RED LED OFF\r\n");
                    }
                }
                else if(isMqttCommand(&topic, "blue", 2))
                {
                    if(isMqttCommand(&payload, "on", 0))
                    {
                        setPinValue(BLUE_LED, 1); // Blue LED ON
                        sendUart0String("  BLUE LED ON\r\n");
                    }
                    else if(isMqttCommand(&payload, "off", 0))
                    {
                        setPinValue(BLUE_LED, 0); // Blue LED OFF
                        sendUart0String("  BLUE LED OFF\r\n");
                    }
                }

            }
            else if(isMqttCommand(&topic, "pb", 1) && isMqttCommand(&topic, "status", 2)) // Print To Terminal Status of Pushbutton
            {
                char buffer[25];

                strcpy(buffer, getFieldString(&payload, 0));
                sendUart0String(buffer);
                sendUart0String("\r\n");
            }

            pubMessageReceived = false;
            resetUserInput(&topic);
            resetUserInput(&payload);
        }
        else if(pubMessageReceived)
        {
            pubMessageReceived = false;
            resetUserInput(&topic);
            resetUserInput(&payload);
        }

        if(!(GPIO_PORTF_DATA_R && PUSH_BUTTON)) // Check to see if SW1 Pressed
        {
            char str[25];
            char buffer[25];

            sprintf(str, "%s", "env/pb");
            sprintf(buffer, "%s", "ButtonPressed");
            mqttPublish(data, 0x5018, str, buffer);
        }

        /* Start of CLI Commands */
        if(userInput.endOfString && isCommand(&userInput, "dhcp", 2))
        {
            char *token;

            token = getFieldString(&userInput, 1); // Retrieve DHCP command from user

            if(strcmp(token, "ON") == 0)           // Enables DHCP mode and stores the mode persistently in EEPROM
            {
                if(!dhcpEnabled)
                {
                    etherEnableDhcpMode();
                    writeEeprom(0x0010, (uint32_t)dhcpEnabled);
                    releaseRequest = true;
                }
            }
            else if(strcmp(token, "OFF") == 0) // Disables DHCP mode and stores the mode persistently in EEPROM
            {
                if(dhcpEnabled)
                {
                    resetAllTimers();                // Turn off all clocks
                    writeEeprom(0x0010, 0xFFFFFFFF); // Erase DHCP Mode in EEPROM
                    sendDhcpReleaseMessage(data);
                    setStaticNetworkAddresses();     // Update ifconfig
                    etherDisableDhcpMode();
                    dhcpIpLeased = false;
                    sendDhcpInformMessage(data);
                }
            }
            else if(strcmp(token, "REFRESH") == 0) // Refresh Current IP address (if in DHCP mode)
            {
                if(dhcpEnabled)
                {
                    renewRequest = true;
                    rebindRequest = releaseRequest = false;
                    dhcpRequestType = 2;
                }
            }
            else if(strcmp(token, "RELEASE") == 0) // Release Current IP address (if in DHCP mode)
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

            if(!dhcpEnabled && strcmp(token, "IP") == 0)      // Set Internet Protocol address
            {
                etherSetIpAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0011);
            }
            else if(!dhcpEnabled && strcmp(token, "GW") == 0) // Set Gateway address
            {
                etherSetIpGatewayAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0012);
            }
            else if(!dhcpEnabled && strcmp(token, "DNS") == 0) // Set Domain Name System address
            {
                setDnsAddress(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0013);
            }
            else if(!dhcpEnabled && strcmp(token, "SN") == 0) // Set Sub-net Mask
            {
                etherSetIpSubnetMask(add1, add2, add3, add4);
                storeAddressEeprom(add1, add2, add3, add4, 0x0014);
            }
            else if(strcmp(token, "MQTT") == 0)              // Set Sub-net Mask
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
        else if(userInput.endOfString && isCommand(&userInput, "publish", 3))
        {
            char str[80];
            char buffer[80];

            // Retrieve network configuration parameter
            strcpy(str,getFieldString(&userInput, 1));

            // Retrieve network configuration parameter
            strcpy(buffer,getFieldString(&userInput, 2));

            // MQTT Publish Packet
            mqttPublish(data, 0x5018, str, buffer);

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "subscribe", 2))
        {
            char buffer[80];
            uint8_t index;

            // Copy MQTT Topic to subscribe to
            strcpy(buffer, getFieldString(&userInput, 1));

            // Find empty slot in subscribed to table
            index = findEmptySlot();

            // Copy Subscription to Table
            strcpy(topics[index].subs, buffer);
            topics[index].validBit = true;

            // Send MQTT Subscribe Packet
            mqttSubscribe(data, 0x5018, buffer);

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "unsubscribe", 2))
        {
            char buffer[80];

            // Retrieve network configuration parameter
            strcpy(buffer, getFieldString(&userInput, 1));

            // Remove Topic from Subscription Table
            createEmptySlot(buffer);

            // Send MQTT Unsubscribe Packet
            mqttUnsubscribe(data, 0x5018, buffer);

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "connect", 1))
        {
            // Mark sendMqttConnect as TRUE to initiate sending of MQTT Connect Packet
            sendMqttConnect = true;

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "disconnect", 1))
        {
            // Send MQTT Disconnect Packet
            mqttDisconnectMessage(data, 0x5018);

            // Change TCP State
            establishedState = false;
            closeState = true;

            // Stop Ping Request Timer
            stopTimer(mqttPing);

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "help", 1))
        {
            char token[MAX_CHARS + 1];

            // Retrieve network configuration parameter
            strcpy(token, getFieldString(&userInput, 1));

            if(strcmp(token, "Inputs") == 0)
            {
                printHelpInputs(); // Print help Inputs
            }
            else if(strcmp(token, "Outputs") == 0)
            {
                printHelpOututs(); // Print help Outputs
            }
            else if(strcmp(token, "Subs") == 0)
            {
                printSubscribedTopics(); // Print Subscribed Topics
            }
            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "reboot", 1))
        {
            // Ensure no Read or Writes to EEPROM are occuring
            while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);

            sendUart0String("Rebooting System...\r\n");
            rebootFlag = true;

            resetUserInput(&userInput);
        }
        else if(userInput.endOfString && isCommand(&userInput, "reset", 1))
        {
            writeEeprom(0x0015, 0xFFFFFFFF); // Erase DHCP Mode in EEPROM
        }
        else if(userInput.endOfString)
        {
            resetUserInput(&userInput);
        }
    }

    return 0;
}
