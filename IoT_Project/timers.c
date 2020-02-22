/*
 * timers.c
 *
 *  Created on: Feb 20, 2020
 *      Author: William Bozarth
 */

#include "timers.h"
#include "uart.h"

//
void initWideTimers()
{
    // Enable clocks
    enablePort(PORTD);
    _delay_cycles(3);

    GPIO_PORTD_AFSEL_R |= 0x30;                                                  // select alternative functions for FREQ_IN pin
    GPIO_PORTD_PCTL_R  &= ~(GPIO_PCTL_PD4_M  | GPIO_PCTL_PD5_M);                 // map alt fns to FREQ_IN
    GPIO_PORTD_PCTL_R  |= (GPIO_PCTL_PD4_WT4CCP0 | GPIO_PCTL_PD5_WT4CCP1);
    GPIO_PORTD_DEN_R   |= 0x30;                                                  // enable bit 6 for digital input

    WTIMER4_CTL_R  &= ~(TIMER_CTL_TAEN | TIMER_CTL_TBEN);                        // turn-off counter before reconfiguring
    WTIMER4_CFG_R  = 4;                                                          // configure as 32-bit counter
    WTIMER4_TAMR_R = (TIMER_TAMR_TAMR_1_SHOT | TIMER_TAMR_TAMIE);                // configure for one-shot mode, count down
    WTIMER4_TBMR_R = (TIMER_TBMR_TBMR_1_SHOT | TIMER_TBMR_TBMIE);
    WTIMER4_CTL_R  |= (TIMER_CTL_TAEVENT_POS | TIMER_CTL_TBEVENT_POS);           // configure for positive edge
    WTIMER4_IMR_R  = (TIMER_IMR_TATOIM | TIMER_IMR_TBTOIM);                      // turn-on interrupts                                                         // zero counter for first period                       // turn-on counter
    NVIC_EN3_R     |= (1 << (INT_WTIMER4A-16-96) | (1 << (INT_WTIMER4B-16-96))); // turn-on interrupt 117 (WTIMER4A) and 118 (WTIMER4B)
}

//
void WideTimer4aIsr()
{
    putsUart0("Renewal timer, T1, expired.\r\n");
    WTIMER4_ICR_R |= TIMER_ICR_TATOCINT; // Clear Time-Out Interrupt Flag of Timer A
}

//
void WideTimer4bIsr()
{
    putsUart0("Rebinding timer, T2, expired.\r\n");
    WTIMER4_ICR_R |= TIMER_ICR_TBTOCINT; // Clear Time-Out Interrupt Flag of Timer B
}

// Function to Load Value for Timer 4A
void setTimer4aValue(uint32_t valueTimer4a)
{
    WTIMER4_TAILR_R = valueTimer4a;   // Load value into Timer A
    WTIMER4_CTL_R  |= TIMER_CTL_TAEN; // Enable interrupts for Timer A
}

// Function to Load Value for Timer 4B
void setTimer4bValue(uint32_t valueTimer4b)
{
    WTIMER4_TBILR_R = valueTimer4b;   // Load value into Timer A
    WTIMER4_CTL_R  |= TIMER_CTL_TBEN; // Enable interrupts for Timer B
}

// Function to disable Timer 4A
void disableTimer4a()
{
    WTIMER4_CTL_R &= ~(TIMER_CTL_TAEN);
}

// Function to disable Timer 4B
void diableTimer4b()
{
    WTIMER4_CTL_R &= ~(TIMER_CTL_TBEN);
}
