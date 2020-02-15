/*
 * terminal.h
 *
 *  Created on: Feb 6, 2020
 *      Author: willi
 */

#ifndef TERMINAL_H_
#define TERMINAL_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "uart.h"

#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS + 1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void getsUart0(USER_DATA* data);
void parseFields(USER_DATA* data);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
uint8_t getStringLength(USER_DATA** data, uint8_t offset);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
void printMainMenu();

#endif /* TERMINAL_H_ */
