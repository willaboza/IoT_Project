/*
 * rtc.h
 *
 *  Created on: May 1, 2020
 *      Author: William Bozarth
 */

#ifndef RTC_H_
#define RTC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"

#define SEC_IN_DAY 86400
#define SEC_IN_HOUR 3600
#define SEC_IN_MIN 60
#define MIN_IN_HOUR 60
#define HOUR_IN_DAY 24
#define MONTH_IN_YEAR 12

typedef struct _timeFrame
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} timeFrame;

void initRtc();
uint32_t getRtcCounter();
void rtcDisable();
void getCurrentTime();
void rtcIsr();


#endif /* RTC_H_ */
