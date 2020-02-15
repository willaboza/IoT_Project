/*
 * eeprom.h
 *
 *  Created on: Feb 12, 2020
 *      Author: willi
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include "tm4c123gh6pm.h"

void initEeprom();
void writeEeprom(uint16_t add, uint32_t data);
uint32_t readEeprom(uint16_t add);
void readDeviceConfig();

#endif /* EEPROM_H_ */
