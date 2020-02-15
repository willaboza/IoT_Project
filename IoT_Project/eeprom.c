/*
 * eeprom.c
 *
 *  Created on: Feb 12, 2020
 *      Author: willi
 */

#include "eeprom.h"

// Function to initialize EEPROM
void initEeprom()
{
    SYSCTL_RCGCEEPROM_R = 1;
    _delay_cycles(3);
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
}

// Function to write data to EEPROM
void writeEeprom(uint16_t add, uint32_t data)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    EEPROM_EERDWR_R = data;
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
}

// Function to read data from EEPROM
uint32_t readEeprom(uint16_t add)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    return EEPROM_EERDWR_R;
}

// Function to determine if DHCP mode ENABLED or DISABLED
void readDeviceConfig()
{
    uint32_t temp;
    uint8_t  *ip;

    if(readEeprom(1) == 0xFFFFFFFF)
    {
        enableDhcpMode();
    }
    else
    {

    }
}
