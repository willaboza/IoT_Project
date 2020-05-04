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

void initAdc();
int16_t readAdc0Ss3();

#endif /* ADC_H_ */
