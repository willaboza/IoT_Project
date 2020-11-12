/*
 * timers.h
 *
 *  Created on: Feb 20, 2020
 *      Author: William Bozarth
 */

#ifndef TIMERS_H_
#define TIMERS_H_

#include "timers.h"

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

void initTimer(void);
bool startOneShotTimer(_callback callback, uint32_t seconds);
bool startPeriodicTimer(_callback callback, uint32_t seconds);
bool stopTimer(_callback callback);
bool restartTimer(_callback callback);
void resetAllTimers(void);
void tickIsr(void);
uint32_t random32(void);
void mqttPing(void);
void clearRedLed(void);
void clearBlueLed(void);
void clearGreenLed(void);

#endif /* TIMERS_H_ */
