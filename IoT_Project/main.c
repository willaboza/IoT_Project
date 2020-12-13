// main.c
// William Bozarth
// Created on: February 6, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "shell.h"
#include "uart0.h"
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
#include "eeprom.h"

// Function to Initialize Hardware
void initHw(void)
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, sysdivider of 5, creating system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);
    enablePort(PORTB);
    _delay_cycles(3);
    enablePort(PORTC);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}

int main(void)
{
    // Declare Variables
    bool ok;
    USER_DATA userInput = {.delimeter = true,
                           .endOfString = false,
                           .fieldCount = 0,
                           .startCount = 0,
                           .characterCount = 0,
    };

    //uint8_t data[MAX_PACKET_SIZE] = {0};

    // Initialize Hardware
    initHw();
    initUart0(115200, 40e6);
    initSpi0(USE_SSI0_RX, 4e6, 40e6);
    initEeprom();
    initTimer();
    //initAdc();
    //initRtc();
    //initWatchdog();

    // Display current ifconfig values and send DHCPREQUEST if Rebooting device
    ok = readDeviceConfig();

    // Output to terminal configuration info
    displayConnectionInfo();

    //Print Main Menu
    printMainMenu();

    // Flash Green LED (on-board)
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    if(ok)
        sendDhcpRequestMessage(data);
    else
    {
        sendArpAnnouncement(data);
        startPeriodicTimer(periodicallyAnnounceAddress, 120 * MULT_FACTOR);
    }

    while(true)
    {
        // Packet processing if available
        if(etherIsDataAvailable())
        {
            if(etherIsOverflow())
            {
                // Set on-board Red LED to alert of Ethernet device overflow
                setPinValue(RED_LED, 1);

                // Start timer to clear Red LED after time has elapsed
                startOneShotTimer(clearRedLed, 3 * MULT_FACTOR);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);

            // Handles IP messages
            if(etherIsIp(data))
            {
                if(etherIsTcp(data)) // Handles TCP packets
                {
                    // Get next TCP state event
                    uint16_t nextTcpEvent = etherIsTcpMsgType(data);

                    // If DHCP msg rx'd then transition to next state
                    (*tcpLookup(nextTcpState, (tcpSysEvent)nextTcpEvent))(data, nextTcpEvent);
                }
                else if(etherIsDhcp(data)) // Handles DHCP messages
                {
                    // Get next DHCP state event
                    dhcpSysEvent nextDhcpEvent = (dhcpSysEvent)dhcpOfferType(data);

                    // If DHCP msg rx'd then transition to next state
                    (*dhcpLookup(nextDhcpState, nextDhcpEvent))(data);
                }
                else if(etherIsArpRequest(data)) // Handle ARP request
                {
                    etherSendArpResponse(data);
                }
                else if(etherIsArpResponse(data)) // Handle ARP response
                {
                    // If ARP Response received before 2 second timer elapses
                    // then send decline message, invalidate IP and use static IP,
                    // wait at least 10 seconds and send another DHCPDISCOVER message.
                    stopTimer(arpResponseTimer);
                    sendDhcpDeclineMessage(data);
                    setStaticNetworkAddresses();
                    startOneShotTimer(waitTimer, 10 * MULT_FACTOR);
                }
            }
        }

        // If User Input detected, then process input
        if(kbhitUart0())
        {
            // Get User Input
            if(getsUart0(&userInput))
            {
                shellCommands(&userInput, data);
            }
            else
            {
                parseFields(&userInput); // Tokenize User Input
            }
        }
    }
}
