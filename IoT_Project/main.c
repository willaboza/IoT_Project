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
    //initWatchdog();

    // Declare Variables
    USER_DATA userInput;
    uint8_t   *udpData;
    uint8_t   data[MAX_PACKET_SIZE];

    // Setup UART0 Baud Rate
    setUart0BaudRate(115200, 40e6);
/*
    // Init Ethernet Interface
    putsUart0("\nStarting eth0\n");
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    etherSetMacAddress(2, 3, 4, 5, 6, 7);
    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 1, 199);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    waitMicrosecond(100000);
    displayConnectionInfo();
    putsUart0("\r\n");
*/

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    resetUserInput(&userInput);

    // Display Main Menu
    printMainMenu();

    while(true)
    {
        // Get User Input
        getsUart0(&userInput);

        // Tokenize User Input
        parseFields(&userInput);

        if(userInput.endOfString && isCommand(&userInput, "dhcp", 2))
        {
            char *token;

            token = getFieldString(&userInput, 1); // Retrieve DHCP command from user

            if(strcmp(token, "on") == 0)           // Enables DHCP mode and stores the mode persistently in EEPROM
            {
                putsUart0("dhcp ON Function.\r\n");
            }
            else if(strcmp(token, "off") == 0)     // Disables DHCP mode and stores the mode persistently in EEPROM
            {
                putsUart0("dhcp OFF Function.\r\n");
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

            if(strcmp(token, "ip") == 0)             // Set Internet Protocol address
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
            putsUart0("ifconfig Function.\r\n");

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
