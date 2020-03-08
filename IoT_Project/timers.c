/*
 * timers.c
 *
 *  Created on: Feb 20, 2020
 *      Author: William Bozarth
 */

#include "timers.h"

bool renewRequest    = false;
bool rebindRequest   = true;
bool releaseRequest  = false;
uint32_t leaseTime   = 0;
uint32_t renewalTime = 0;
uint32_t rebindTime  = 0;

// Function To Initialize Timers
void initTimer()
{
    uint8_t i = 0;

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R4;
    _delay_cycles(3);

    TIMER4_CTL_R   &= ~TIMER_CTL_TAEN;                     // turn-off counter before reconfiguring
    TIMER4_CFG_R   = TIMER_CFG_32_BIT_TIMER;               // configure as 32-bit counter
    TIMER4_TAMR_R  = TIMER_TAMR_TAMR_PERIOD;               // configure for one-shot mode, count down
    TIMER4_TAILR_R = 40000000;
    TIMER4_CTL_R   |= TIMER_CTL_TAEN;
    TIMER4_IMR_R   |= TIMER_IMR_TATOIM;                    // enable interrupts
    NVIC_EN2_R     |= (1 << (INT_TIMER4A-80));             // turn-on interrupt 86 (TIMER4A)

    for(i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i]  = 0;
        fn[i]     = 0;
        reload[i] = false;
    }
}

//
bool startOneShotTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found     = fn[i] == NULL;
        period[i] = seconds;
        ticks[i]  = seconds;
        fn[i]     = callback;
        reload[i] = false;
        i++;
    }
    return found;
}

//
bool startPeriodicTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found     = fn[i] == NULL;
        period[i] = seconds;
        ticks[i]  = seconds;
        fn[i]     = callback;
        reload[i] = false;
        i++;
    }
    return found;
}

//
bool stopTimer(_callback callback)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found = fn[i] == callback;
        if(found)
        {
            ticks[i] = 0;
        }
        i++;
    }
    return found;
}

bool restartTimer(_callback callback)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found = fn[i] == callback;
        if(found)
        {
            ticks[i] = period[i];
        }
        i++;
    }
    return found;
}
void tickIsr()
{
    /*
    uint8_t i = 0;
    bool found = false;
    for(i = 0; i < NUM_TIMERS; i++)
    {
        if(ticks[i] != 0)
        {
            ticks[i]--;
            if(ticks[i] == 0)
            {
                if(reload[i])
                {
                    ticks[i] = period[i];
                    (*fn[i])();
                }
            }
            TIMER4_ICR_R = TIMER_ICR_TATOCINT;
        }
        i++;
    }
    */
}
