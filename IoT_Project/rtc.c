/*
 * rtc.c
 *
 *  Created on: May 1, 2020
 *      Author: William Bozarth
 */

#include "rtc.h"

// Initialization and Configuration of Hibernation Module
void initRtc()
{
    // Enable clocks
    SYSCTL_RCGCHIB_R |= SYSCTL_RCGCHIB_R0;
    _delay_cycles(3);

    // interrupt mask register to enable the WC interrupt.
    HIB_IM_R |= HIB_IM_WC;

    // Wait until the WC interrupt in the HIBMIS register has been triggered before performing any other.
    while(!(HIB_CTL_R & HIB_CTL_WRC));

    HIB_RTCT_R = 0x7FFF; // Increase trim by 14 clock cycles to slow down Hibernation Real-time clock.

    while(!(HIB_CTL_R & HIB_CTL_WRC)); // Wait for interrupt to clear

    // Disable RTCALT0 by Clearing its bit to turn off matching before turning on the interrupt
    rtcDisable();

    NVIC_EN1_R |= 1 << (INT_HIBERNATE-16-32); // turn-on interrupt 43 (Hibernation Module)

    HIB_CTL_R = HIB_CTL_CLK32EN | HIB_CTL_RTCEN; // Turn on the clock enable and RTC enable bits

    while(!(HIB_CTL_R & HIB_CTL_WRC)); // Spin until the write complete bit is set
}

void rtcDisable()
{
    HIB_IM_R &= ~(HIB_IM_RTCALT0); // Turn off the RTC enable bit.
}

// Function to Get Current Time
void getCurrentTime()
{
    uint8_t hours = 0;
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    uint32_t mod = 0;
    uint32_t temp = 0;

    // Determine difference between current count and initial count
    deltaTime = (getHibRtcCounter() - startHibRtcCount);

    // Determine number of days elapsed as well as remaining hours
    mod = (deltaTime % SEC_IN_DAY);

    // Determine number of hours elapsed as well as remaining minutes
    hours = (mod / SEC_IN_HOUR);
    mod = (mod % SEC_IN_HOUR);

    // Determine number of minutes elapsed as well as seconds elapsed
    minutes = (mod / SEC_IN_MIN);
    seconds = (mod % SEC_IN_MIN);

    // Determine Current Seconds.
    temp = ((startSec + seconds) / SEC_IN_MIN);
    currentSec = (startSec + seconds) % SEC_IN_MIN;

    // Determine current minutes.
    currentMin = ((startMin + minutes + temp) % MIN_IN_HOUR);
    temp = ((startMin + minutes + temp) / MIN_IN_HOUR);

    // Determine current Hours.
    currentHour = (startHour + hours + temp) % HOUR_IN_DAY;
}

// Hibernation Interrupt routine to execute
void rtcIsr()
{
    HIB_IC_R = HIB_IC_WC | HIB_IC_RTCALT0; // Clear interrupts for HIB Real-time clock
}
