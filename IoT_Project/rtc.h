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

typedef struct _timeFrame
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} timeFrame;

void initRtc();
void rtcDisable();


#endif /* RTC_H_ */
