/*
 * adc.h
 *
 *  Created on: May 1, 2020
 *      Author: William Bozarth
 */

#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"

#define TEMP_OFFSET 1600
#define TEMP_SENS 321

void initAdc();
int16_t readAdc0Ss3();
uint8_t instantTemp();

#endif /* ADC_H_ */
