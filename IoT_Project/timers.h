/*
 * timers.h
 *
 *  Created on: Feb 20, 2020
 *      Author: William Bozarth
 */

#ifndef TIMERS_H_
#define TIMERS_H_

//List of Libraries to Include
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "uart.h"

#define NUM_TIMERS 10

extern bool renewRequest;
extern bool rebindRequest;
extern bool releaseRequest;
extern uint32_t leaseTime;
extern uint32_t renewalTime;
extern uint32_t rebindTime;

typedef void(*_callback)(void);
_callback fn[NUM_TIMERS];

uint32_t period[NUM_TIMERS];
uint32_t ticks[NUM_TIMERS];
bool reload[NUM_TIMERS];

void initTimer();
bool startOneShotTimer();
bool startPeriodicTimer();
bool stopTimer();
bool restartTimer();
void tickIsr();
uint32_t random32();


#endif /* TIMERS_H_ */
