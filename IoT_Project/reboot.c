// Watchdog Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz


// Watchdog counter 0:
//   This counter is driven by system clock

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include "reboot.h"

// Initialize WATCHDOG
void initWatchdog()
{
    // Enable clocks
    SYSCTL_RCGCWD_R |= SYSCTL_RCGCWD_R0;
    _delay_cycles(3);

    // Configure WDT0 which is driven by the system clock
    WATCHDOG0_LOAD_R = (uint32_t)TIMEOUT_MS * 40e3;      // convert into fcyc units
    WATCHDOG0_CTL_R |= WDT_CTL_RESEN;                    // enable reset if timeout
    WATCHDOG0_CTL_R |= WDT_CTL_INTEN;                    // enable interrupts
    WATCHDOG0_LOCK_R = 0x1ACCE551;                       // lock-out further changes
    WATCHDOG0_ICR_R = 0;                                 // clear any pending interrupt
    NVIC_EN0_R |= 1 << (INT_WATCHDOG-16);                // turn-on interrupt 34 (WATCHDOG)
}

// Function to reset watchdog timer before system reboots
void resetWatchdog()
{
    WATCHDOG0_LOAD_R = (uint32_t)TIMEOUT_MS * 40e3;      // convert into PIOOSC units
    WATCHDOG0_ICR_R = 0;
}

// Watchdog timer ISR
void watchdogIsr()
{
    //This is the last chance to avoid a reset
    // Write to ICR to avoid a reset
    if(rebootFlag == true)
    {
        resetWatchdog();
    }
}
