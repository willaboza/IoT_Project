/*
 * pwm0.h
 *
 *  Created on: May 1, 2020
 *      Author: William Bozarth
 */

#ifndef PWM0_H_
#define PWM0_H_

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"

#define MAX_PWM 1023

#define PWM0_RED_LED PORTF,1
#define PWM0_BLUE_LED PORTF,2
#define PWM0_GREEN_LED PORTF,3

void initPwm0();
void setRgbColor(uint16_t red, uint16_t green, uint16_t blue);
int normalizeRgbColor(int measurement);
int scaleRgbColor();
void setRedLed(uint16_t red);
void setGreenLed(uint16_t green);
void setBlueLed(uint16_t blue);

#endif /* PWM0_H_ */
