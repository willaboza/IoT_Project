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
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "uart.h"
#include "ethernet.h"
#include "tcp.h"
#include "mqtt.h"


#define MAX_CHARS 80
#define MAX_FIELDS 8

typedef struct _userData
{
    bool    delimeter;
    bool    endOfString;
    uint8_t fieldCount;
    uint8_t characterCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char    fieldType[MAX_FIELDS];
    char    buffer[MAX_CHARS + 1];
} userData;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void getsUart0(userData* data);
void parseFields(userData* data);
void resetUserInput(userData* data);
void printMainMenu();
char* getFieldString(userData* data, uint8_t fieldNumber);
int32_t getFieldInteger(userData* data, uint8_t fieldNumber);
uint8_t getStringLength(userData** data, uint8_t offset);
bool isCommand(userData* data, const char strCommand[], uint8_t minArguments);
void processMqttMessage(uint8_t packet[], userData* topic, userData* data);
void parseMqttPacket(userData* data);
bool isMqttCommand(userData* data, const char strCommand[], uint8_t minArguments);
void printHelpInputs();
void printHelpOututs();
void printSubscribedTopics();

#endif /* TERMINAL_H_ */
