// eeprom.c
// William Bozarth
// Created on: February 12, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#include "eeprom.h"

// Function to initialize EEPROM
void initEeprom()
{
    SYSCTL_RCGCEEPROM_R = 1;
    _delay_cycles(6);
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);
}

// Function to write data to EEPROM
void writeEeprom(uint16_t add, uint32_t data)
{
    EEPROM_EEBLOCK_R = add >> 4; // Shift right 4 bits is same as dividing address by 16
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

// Gets either IP, GW, SN, or MQTT address stored in EEPROM
void getAddressInfo(uint8_t add[], uint8_t mem, uint8_t SIZE)
{
    uint8_t i;
    uint32_t num = readEeprom(mem);

    // Return if no address stored at memory location
    if(num == 0xFFFFFFFF)
        return;

    // Retrieve stored address
    for(i = 0; i < SIZE; i++)
    {
        if(i % 4 == 0 && i > 0)
        {
            mem += 4;
            num = readEeprom(mem);
        }

        add[i] = (num >> (24 - (i % 4) * 8));
    }
}

// Function to store Address in EEPROM
void storeAddressEeprom(uint8_t add[], uint16_t block, uint8_t SIZE)
{
    uint8_t i;
    uint32_t num = 0;

    // Store Address in EEPROM
    for(i = 0; i < SIZE; i++)
    {
        num |= (add[i] << (24 - (i % 4) * 8));

        if((i + 1) % 4 == 0 || i == SIZE - 1)
        {
            writeEeprom(block, num);
            block += 4;
            num = 0;
        }
    }
}

// Function "erases" perviously stored values in EEPROM
void eraseAddressEeprom(void)
{
    writeEeprom(0x0010, 0xFFFFFFFF); // DHCP Mode
    writeEeprom(0x0011, 0xFFFFFFFF); // IP
    writeEeprom(0x0012, 0xFFFFFFFF); // GW
    writeEeprom(0x0013, 0xFFFFFFFF); // DNS
    writeEeprom(0x0014, 0xFFFFFFFF); // SN
}
