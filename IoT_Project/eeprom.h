// eeprom.h
// William Bozarth
// Created on: February 12, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include "tm4c123gh6pm.h"

void initEeprom(void);
void writeEeprom(uint16_t add, uint32_t data);
uint32_t readEeprom(uint16_t add);
void getAddressInfo(uint8_t add[], uint8_t mem, uint8_t SIZE);
void storeAddressEeprom(uint8_t add[], uint16_t block, uint8_t SIZE);
void eraseAddressEeprom(void);

#endif /* EEPROM_H_ */
