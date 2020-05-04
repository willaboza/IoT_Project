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
extern bool arpResponseRx;
extern bool sendMqttPing;
extern uint32_t leaseTime;
extern uint8_t dhcpRequestType;

typedef void(*_callback)(void);
extern _callback fn[NUM_TIMERS];
extern uint32_t period[NUM_TIMERS];
extern uint32_t ticks[NUM_TIMERS];
extern bool reload[NUM_TIMERS];
extern uint8_t dhcpRequestsSent;

void initTimer();
bool startOneShotTimer(_callback callback, uint32_t seconds);
bool startPeriodicTimer(_callback callback, uint32_t seconds);
bool stopTimer(_callback callback);
bool restartTimer(_callback callback);
void resetAllTimers();
void tickIsr();
uint32_t random32();
void renewalTimer();
void rebindTimer();
void arpResponseTimer();
void waitTimer();
void renewRetransmitTimer();
void rebindRetransmitTimer();
void mqttPing();


#endif /* TIMERS_H_ */
