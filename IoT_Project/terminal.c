/*
 * terminal.c
 *
 *  Created on: Feb 6, 2020
 *      Author: William Bozarth
 */

#include "terminal.h"

// Function to get Input from Terminal
void getsUart0(userData* data)
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
        }
    }
}

// Function to Ping RequestTokenize Strings
void parseFields(userData* data)
{
    char    c;
    uint8_t count, fieldIndex;

    count = data->characterCount;

    c = data->buffer[count];

    fieldIndex = data->fieldCount;

    if(c != '\0' && count <= MAX_CHARS)
    {
        if(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('/' == c) || ('#' == c)) // Verify is character is an alpha (case in-sensitive)
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

// Function to parse MQTT Packets when received
void parseMqttPacket(userData* data)
{
    char    c;
    uint8_t count, fieldIndex;

    count = 0;

    c = data->buffer[count];

    fieldIndex = data->fieldCount;

    while(c != '\0' && count <= MAX_CHARS)
    {
        if(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('#' == c))
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
        c = data->buffer[count];
    }
}

// Function to Return a Token as a String
char* getFieldString(userData* data, uint8_t fieldNumber)
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
int32_t getFieldInteger(userData* data, uint8_t fieldNumber)
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
bool isCommand(userData* data, const char strCommand[], uint8_t minArguments)
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

// Function to process incoming MQTT Messages
void processMqttMessage(uint8_t packet[], userData* topic, userData* data)
{
    uint16_t msgLength, topicLength, payloadLength, k,i;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    msgLength = mqtt->packetLength; // Get Message Length

    i = 0;
    topicLength = mqtt->data[i++] << 8; // Topic Length MSB
    topicLength = mqtt->data[i++];      // Topic Length LSB

    k = 0;
    // Process Topic of PUBLISH Packet
    while(k < topicLength)
    {
        topic->buffer[k++] = mqtt->data[i++];
    }
    topic->buffer[k++] = '\0';
    topic->characterCount = k;

    // If QoS Level Set then Look for Packet Identifier
    if((mqtt->control & 0x06) == 0x02 || (mqtt->control & 0x0F) == 0x04)
    {
        packetId = mqtt->data[i++] << 8; // Packet Identifier MSB
        packetId = mqtt->data[i++];      // Packet Identifier LSB

        if((mqtt->control & 0x06) == 0x02) // QoS = 1
        {
            sendPubackFlag = true;
        }
        else if((mqtt->control & 0x0F) == 0x04) // QoS = 2
        {
            sendPubrecFlag = true;
        }
    }

    payloadLength = msgLength - topicLength - 2;

    k = 0;
    // Process Payload of PUBLISH Packet
    while(k < payloadLength)
    {
        data->buffer[k++] = mqtt->data[i++];
    }
    data->buffer[k++] = '\0';
    data->characterCount = k;
}

// Function Used to Determine if Correct Command Entered
bool isMqttCommand(userData* data, const char strCommand[], uint8_t minArguments)
{
    bool    command = false;
    uint8_t offset = 0, index = 0;
    char    copy[MAX_CHARS + 1];

    offset = data->fieldPosition[minArguments];

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
void resetUserInput(userData* data)
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
    sendUart0String("  help Inputs\r\n");
    sendUart0String("  help Outputs\r\n");
    sendUart0String("  help Subs\r\n");
    sendUart0String("  reboot\r\n");
}

// Function to Print Help Inputs
void printHelpInputs()
{
    sendUart0String("\r\n");
    sendUart0String("  Local Input Topics to MQTT Client:\r\n");
    sendUart0String("    env/pb\r\n");
    sendUart0String("    env/temp\r\n");
    sendUart0String("    env/led/green on|off\r\n");
    sendUart0String("    env/led/red   on|off\r\n");
    sendUart0String("    env/led/blue  on|off\r\n");
}

// Function to Print Help Outputs
void printHelpOututs()
{
    sendUart0String("\r\n");
    sendUart0String("  Local Output Topics to MQTT Client:\r\n");
    sendUart0String("    env/uart\r\n");
    sendUart0String("    env/led/green STATUS\r\n");
    sendUart0String("    env/led/red   STATUS\r\n");
    sendUart0String("    env/led/blue  STATUS\r\n");
}

// Function to Print Current Subscribed topics
void printSubscribedTopics()
{
    uint8_t i = 0;
    char str[MAX_SUB_CHARS];

    sendUart0String("List of Subscribed Topics:\r\n");
    while(i < MAX_TABLE_SIZE)
    {
        if(topics[i].validBit)
        {
            sprintf(str, "  %s", topics[i].subs);
            sendUart0String(str);
            sendUart0String("\r\n");
        }
        i++;
    }
}
