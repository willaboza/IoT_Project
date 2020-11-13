// shell.h
// William Bozarth
// Created on: October 7, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

#include "shell.h"

//
// Defines for Constants
//
#define MAX_CHARS  50
#define MAX_FIELDS 5

//
// Structure Definition
//
typedef struct _USER_DATA
{
    bool    delimeter;
    bool    endOfString;
    uint8_t fieldCount;
    uint8_t startCount;
    uint8_t characterCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char    fieldType[MAX_FIELDS];
    char    buffer[MAX_CHARS];
} USER_DATA;

//
// Definitions
//
bool getsUart0(USER_DATA* data);
void parseFields(USER_DATA* data);
void resetUserInput(USER_DATA* data);
bool isCommand(USER_DATA** data, const char strCommand[], uint8_t minArguments);
void getFieldString(USER_DATA** data, char fieldString[], uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA** data, uint8_t fieldNumber);
void parseMqttPacket(USER_DATA* data);
void processMqttMessage(uint8_t packet[], USER_DATA* topic, USER_DATA* data);
bool isMqttCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
void printSubscribedTopics(void);
void shellCommands(USER_DATA* userInput, uint8_t data[]);

#endif /* SHELL_H_ */
