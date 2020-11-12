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

void initEeprom(void);
void writeEeprom(uint16_t add, uint32_t data);
uint32_t readEeprom(uint16_t add);
void getAddressInfo(uint8_t add[], uint8_t mem, uint8_t SIZE);
void storeAddressEeprom(uint8_t add1, uint8_t add2, uint8_t add3, uint8_t add4, uint16_t block);
void eraseAddressEeprom(void);

#endif /* EEPROM_H_ */
