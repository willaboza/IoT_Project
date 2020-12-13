// timer.c
// William Bozarth
// Created on: February 20, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Timer 4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "uart0.h"
#include "timers.h"

uint8_t dhcpRequestsSent = 0;
uint8_t dhcpRequestType  = 0;
uint32_t leaseTime  = 0;
//bool arpResponseRx  = false;
//bool sendMqttPing = false;

bool reload[NUM_TIMERS]     = {0};
uint32_t period[NUM_TIMERS] = {0};
uint32_t ticks[NUM_TIMERS]  = {0};
_callback fn[NUM_TIMERS]    = {0};

// Function To Initialize Timers
void initTimer(void)
{
    uint8_t i;

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R4;
    _delay_cycles(3);

    TIMER4_CTL_R   &= ~TIMER_CTL_TAEN;       // turn-off counter before reconfiguring
    TIMER4_CFG_R   = TIMER_CFG_32_BIT_TIMER; // configure as 32-bit counter
    TIMER4_TAMR_R  = TIMER_TAMR_TAMR_PERIOD; // configure for one-shot mode, count down
    //TIMER4_TAILR_R = 40000000;
    TIMER4_TAILR_R = 40000;                  // Need to have a 1 kHz clock
    TIMER4_CTL_R   |= TIMER_CTL_TAEN;
    TIMER4_IMR_R   |= TIMER_IMR_TATOIM;      // enable interrupts
    NVIC_EN2_R     |= 1 << (INT_TIMER4A-80); // turn-on interrupt 86 (TIMER4A)

    // Set initial timer values
    for(i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i]  = 0;
        fn[i]     = NULL;
        reload[i] = false;
    }
}

// Function to Start One Shot Timer
bool startOneShotTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;

    while(i < NUM_TIMERS)
    {
        if (fn[i] == NULL)
        {
            period[i] = seconds;
            ticks[i]  = seconds;
            fn[i]     = callback;
            reload[i] = false;
            return true;
        }
        i++;
    }
    return false;
}

// Function to Start Periodic Timer
bool startPeriodicTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;

    while(i < NUM_TIMERS)
    {
        if (fn[i] == NULL)
        {
            period[i] = seconds;
            ticks[i]  = seconds;
            fn[i]     = callback;
            reload[i] = true;
            return true;
        }
        i++;
    }
    return false;
}

//
bool stopTimer(_callback callback)
{
    uint8_t i = 0;

    for(i = 0; i < NUM_TIMERS; i++)
    {
        if(fn[i] == callback)
        {
            period[i] = 0;
            ticks[i]  = 0;
            fn[i]     = NULL;
            reload[i] = false;
            return true;
        }
    }
    return false;
}

// Restart Timer Previously Initialized
bool restartTimer(_callback callback)
{
    uint8_t i = 0;
    while(i < NUM_TIMERS)
    {
        if(fn[i] == callback)
        {
            ticks[i] = period[i];
            return true;
        }
        i++;
    }
    return false;
}

// Reset all timers
void resetAllTimers(void)
{
    uint8_t i;
    for(i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i]  = 0;
        fn[i]     = NULL;
        reload[i] = false;
    }
}

// Function to handle Timer Interrupts
void tickIsr(void)
{
    uint8_t i;
    for (i = 0; i < NUM_TIMERS; i++)
    {
        if (ticks[i] > 0)
        {
            ticks[i]--;
            if (ticks[i] == 0)
            {
                if (reload[i])
                    ticks[i] = period[i];
                (*fn[i])();
            }
        }
    }

    TIMER4_ICR_R = TIMER_ICR_TATOCINT;
}

// Placeholder random number function
uint32_t random32(void)
{
    return TIMER4_TAV_R;
}

// Turn off on board Red LED after elapsed time
void clearRedLed(void)
{
    stopTimer(clearRedLed);

    // Turn off Red LED
    setPinValue(RED_LED, 0);
}

// Turn off on board Blue LED after elapsed time
void clearBlueLed(void)
{
    stopTimer(clearBlueLed);

    // Turn off Red LED
    setPinValue(BLUE_LED, 0);
}

// Turn off on board Green LED after elapsed time
void clearGreenLed(void)
{
    stopTimer(clearGreenLed);

    // Turn off Red LED
    setPinValue(GREEN_LED, 0);
}
