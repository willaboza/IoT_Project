/*
 * terminal.c
 *
 *  Created on: Feb 6, 2020
 *      Author: William Bozarth
 */

#include "terminal.h"

// Function to get Input from Terminal
void getsUart0(USER_DATA* data)
{
    uint8_t count;
    char    c;

    count = data->characterCount;

    c = getcUart0();
    UART0_ICR_R = 0xFFF; // Clear any interrupts
    if((c == 13) || (count == MAX_CHARS))
    {
        data->buffer[count++] = '\0';
        data->endOfString = true;
        sendUart0String("\r\n");
    }
    else
    {
        if ((c == 8 || c == 127) && count > 0) // Decrement count if invalid character entered
        {
            count--;
            data->characterCount = count;
        }
        if (c >= ' ' && c < 127)
        {
            data->buffer[count] = c;
            /*
            if('A' <= c && c <= 'Z')
            {
                data->buffer[count] = c + 32;  // Converts capital letter to lower case
            }
            else
            {
                data->buffer[count] = c;
            }
            */
        }
    }
}

// Function to Ping RequestTokenize Strings
void parseFields(USER_DATA* data)
{
    char    c;
    uint8_t count, fieldIndex;

    count = data->characterCount;

    c = data->buffer[count];

    fieldIndex = data->fieldCount;

    if(c != '\0' && count <= MAX_CHARS)
    {
        if((('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('/' == c)) && !pubMessageReceived) // Verify is character is an alpha (case in-sensitive)
        {
            if(data->delimeter == true)
            {
                data->fieldPosition[fieldIndex] = count;
                data->fieldType[fieldIndex] = 'A';
                data->fieldCount = ++fieldIndex;
                data->delimeter = false;
            }
            data->characterCount = ++count;
        }
        else if(('0' <= c && c <= '9') || ',' == c) //Code executes for numerics same as alpha
        {
            if(data->delimeter == true)
            {
                data->fieldPosition[fieldIndex] = count;
                data->fieldType[fieldIndex] = 'N';
                data->fieldCount = ++fieldIndex;
                data->delimeter = false;
            }
            data->characterCount = ++count;
        }
        else // Insert NULL('\0') into character array if NON-alphanumeric character detected
        {
            data->buffer[count] = '\0';
            data->characterCount = ++count;
            data->delimeter = true;
        }
    }
}

// Function to Return a Token as a String
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    uint8_t offset, index = 0;
    char    string[MAX_CHARS + 1];

    offset = data->fieldPosition[fieldNumber]; // Get position of first character in string of interest

    while(data->buffer[offset] != '\0')
    {
        string[index++] = data->buffer[offset++];
    }

    string[index] = '\0';

    return (char*)string;
}

// Function to Return a Token as an Integer
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    int32_t num;
    uint8_t offset, index = 0;
    char    copy[MAX_CHARS + 1];

    offset = data->fieldPosition[fieldNumber];  // Get position of first character in string of interest

    while(data->buffer[offset] != '\0')
    {
        copy[index++] = data->buffer[offset++];
    }

    copy[index] = '\0';

    num = atoi(copy);

    return num;
}

// Function Used to Determine if Correct Command Entered
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    bool    command = false;
    uint8_t offset = 0, index = 0;
    char    copy[MAX_CHARS + 1];

    while(data->buffer[offset] != '\0')
    {
        copy[index++] = data->buffer[offset++];
    }

    copy[index] = '\0';

    if((strcmp(strCommand, copy) == 0) && (data->fieldCount >= minArguments))
    {
        command = true;
    }

    return command;
}

// Function to reset User Input for next command
void resetUserInput(USER_DATA* data)
{
    data->characterCount = 0;
    data->fieldCount = 0;
    data->delimeter = true;
    data->endOfString = false;
}

// Function to Print Main Menu
void printMainMenu()
{
    sendUart0String("Commands:\r\n");
    sendUart0String("  dhcp ON|OFF|REFRESH|RELEASE\r\n");
    sendUart0String("  set IP|GW|DNS|SN|MQTT w.x.y.z\r\n");
    sendUart0String("  ifconfig\r\n");
    sendUart0String("  publish TOPIC DATA\r\n");
    sendUart0String("  subscribe TOPIC\r\n");
    sendUart0String("  unsubscribe TOPIC\r\n");
    sendUart0String("  connect\r\n");
    sendUart0String("  disconnect\r\n");
    sendUart0String("  reboot\r\n");
}
