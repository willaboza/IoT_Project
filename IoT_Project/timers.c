// Timer Service Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Timer 4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include "timers.h"

uint8_t dhcpRequestsSent = 0;
uint8_t dhcpRequestType  = 0;

bool renewRequest   = false;
bool rebindRequest  = true;
bool releaseRequest = false;
uint32_t leaseTime  = 0;
bool arpResponseRx  = false;

bool reload[NUM_TIMERS]     = {0};
uint32_t period[NUM_TIMERS] = {0};
uint32_t ticks[NUM_TIMERS]  = {0};
_callback fn[NUM_TIMERS]    = {0};

// Function To Initialize Timers
void initTimer()
{
    uint8_t i;

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R4;
    _delay_cycles(3);

    TIMER4_CTL_R   &= ~TIMER_CTL_TAEN;                     // turn-off counter before reconfiguring
    TIMER4_CFG_R   = TIMER_CFG_32_BIT_TIMER;               // configure as 32-bit counter
    TIMER4_TAMR_R  = TIMER_TAMR_TAMR_PERIOD;               // configure for one-shot mode, count down
    TIMER4_TAILR_R = 40000000;
    TIMER4_CTL_R   |= TIMER_CTL_TAEN;
    TIMER4_IMR_R   |= TIMER_IMR_TATOIM;                    // enable interrupts
    NVIC_EN2_R     |= 1 << (INT_TIMER4A-80);             // turn-on interrupt 86 (TIMER4A)

    for(i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i]  = 0;
        fn[i]     = 0;
        reload[i] = false;
    }
}

// Function to Start One Shot Timer
bool startOneShotTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found = fn[i] == NULL;
        if (found)
        {
            period[i] = seconds;
            ticks[i] = seconds;
            fn[i] = callback;
            reload[i] = false;
        }
        i++;
    }
    return found;
}

// Function to Start Periodic Timer
bool startPeriodicTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while(i < NUM_TIMERS && !found)
    {
        found = fn[i] == NULL;
        if (found)
        {
            period[i] = seconds;
            ticks[i] = seconds;
            fn[i] = callback;
            reload[i] = true;
        }
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
            period[i] = 0;
            ticks[i]  = 0;
            fn[i]     = 0;
            reload[i] = false;
        }
        i++;
    }
    return found;
}

// Restart Timer Previously Initialized
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

// Reset all timers
void resetAllTimers()
{
    uint8_t i;
    for(i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i]  = 0;
        fn[i]     = 0;
        reload[i] = false;
    }
    //putsUart0("  All Timers Reset\r\n");
}

// Function to handle Timer Interrupts
void tickIsr()
{
    uint8_t i;
    for (i = 0; i < NUM_TIMERS; i++)
    {
        if (ticks[i] != 0)
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
uint32_t random32()
{
    return TIMER4_TAV_R;
}

//
void renewalTimer()
{
    renewRequest = true;
    rebindRequest = false;
    releaseRequest = false;
    dhcpRequestType = 2; // DHCPREQUEST Type 2 is reserved for RENEWING
    //putsUart0("  T1 Expired\r\n");
}

// Timer 2
void rebindTimer()
{
    stopTimer(rebindTimer);
    renewRequest = true;
    dhcpRequestType = 3; // DHCPREQUEST Type 3 is reserved for REBINDING
    //putsUart0("  T2 Expired\r\n");
}

// 2-Second Timer to wait for any A
void arpResponseTimer()
{
    stopTimer(arpResponseTimer);
    arpResponseRx = false;
    //putsUart0("  ARP Timer Expired\r\n");
}

// DHCP "WAIT" TIMER
void waitTimer()
{
    stopTimer(waitTimer);
    rebindRequest = true;
    renewRequest = releaseRequest = arpResponseRx = false;
    //putsUart0("  10 s WAIT Timer Expired\r\n");
}

