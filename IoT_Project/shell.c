// shell.c
// William Bozarth
// Created on: October 7, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "ethernet.h"
#include "uart0.h"
#include "eeprom.h"
#include "dhcp.h"
#include "shell.h"
#include "reboot.h"
#include "timers.h"
#include "tcp.h"

// Function to get Input from Terminal
bool getsUart0(USER_DATA* data)
{
    char c;

    c = UART0_DR_R & 0xFF; // Get character

    UART0_ICR_R = 0xFFF; // Clear any UART0 interrupts

    // Determine if user input is complete
    if((c == 13) || ((data->characterCount + 1) % MAX_CHARS == data->startCount))
    {
        data->buffer[data->characterCount] = '\0';
        data->startCount = data->characterCount = (data->characterCount + 1) % MAX_CHARS;
        sendUart0String("\r\n");
        return true;
    }

    if(data->characterCount == data->startCount)
    {
        data->fieldCount = 0;
        data->delimeter = true;
    }

    if(c == 8 || c == 127) // Decrement count if invalid character entered
    {
        if(data->characterCount != data->startCount)
        {
            if(data->characterCount != 0)
                data->characterCount--;
            else
                data->characterCount = MAX_CHARS;

            // Removes character from terminal display
            sendUart0String("\b \b");
        }
    }
    else if(c >= ' ' && c < 127) // Converts capital letter to lower case (if necessary)
    {
        if('A' <= c && c <= 'Z')
        {
            data->buffer[data->characterCount] = c + 32;
            data->characterCount = (data->characterCount + 1) % MAX_CHARS;
        }
        else
        {
            data->buffer[data->characterCount] = c;
            data->characterCount = (data->characterCount + 1) % MAX_CHARS;
        }
    }

    return false;
}

// Function to Tokenize Strings
void parseFields(USER_DATA* data)
{
    char    c;
    uint8_t count, fieldIndex;

    count = data->characterCount - 1; // Subtract by one to get correct character count

    c = data->buffer[count];

    // Check if at end of user input and exit function if so,
    // or if backspace or delete was entered by user.
    if(c == '\0' || count > MAX_CHARS || c == 8 || c == 127)
        return;

    // Get current Index for field arrays
    fieldIndex = data->fieldCount;

    if('a' <= c && c <= 'z') // Verify is character is an alpha (case sensitive)
    {
        if(data->delimeter)
        {
            data->fieldPosition[fieldIndex] = count;
            data->fieldType[fieldIndex] = 'A';
            data->fieldCount += 1;
            data->delimeter = false;
        }
    }
    else if(('0' <= c && c <= '9') || ',' == c ||  c == '.') //Code executes for numerics same as alpha
    {
        if(data->delimeter)
        {
            data->fieldPosition[fieldIndex] = count;
            data->fieldType[fieldIndex] = 'N';
            data->fieldCount += 1;
            data->delimeter = false;
        }
    }
    else // Insert NULL('\0') into character array if NON-alphanumeric character detected
    {
        data->buffer[count] = '\0';
        data->delimeter = true;
    }
}

// Function to parse MQTT Packets when received
void parseMqttPacket(USER_DATA* data)
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
void getFieldString(USER_DATA** data, char fieldString[], uint8_t fieldNumber)
{
    uint8_t offset, index = 0;

    // Get position of first character in string of interest
    offset = (*data)->fieldPosition[fieldNumber];

    // Copy characters for return
    while((*data)->buffer[offset] != '\0')
    {
        fieldString[index++] = (*data)->buffer[offset++];
        offset %= MAX_CHARS;
    }

    // Add NULL to terminate string
    fieldString[index] = '\0';
}

// Function to Return a Token as an Integer
int32_t getFieldInteger(USER_DATA** data, uint8_t fieldNumber)
{
    uint8_t offset, index = 0;
    char copy[MAX_CHARS];

    offset = (*data)->fieldPosition[fieldNumber];  // Get position of first character in string of interest

    while((*data)->buffer[offset] != '\0')
    {
        copy[index++] = (*data)->buffer[offset++];
        offset %= MAX_CHARS;
    }

    copy[index] = '\0';

    return atoi(copy);
}

// Function Used to Determine if Correct Command Entered
bool isCommand(USER_DATA** data, const char strCommand[], uint8_t minArguments)
{
    bool ok = false;
    int val;
    uint8_t c1, c2, offset, index = 0;

    if((*data)->fieldCount < minArguments)
        return false;

    offset = (*data)->fieldPosition[0];

    while((c1 = strCommand[index++]) != '\0')
    {
        c2 = (*data)->buffer[offset++];
        val = c1 - c2;

        if(val != 0 || c2 == 0)
            return false;

        offset %= MAX_CHARS;
        ok = true;
    }

    return ok;
}

/*
// Returns value of current argument for shell commands
uint8_t getArgument(USER_DATA** data)
{
    // Handle edge case of no more input commands to process
    if((*data)->fieldCount == 0)
        return 0;


    (*data)->fieldCount--;
}
*/

// Function to process incoming MQTT Messages
void processMqttMessage(uint8_t packet[], USER_DATA* topic, USER_DATA* data)
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
bool isMqttCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
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
void resetUserInput(USER_DATA* data)
{
    data->startCount = 0;
    data->characterCount = 0;
    data->fieldCount = 0;
    data->delimeter = true;
}

// Function to Print Current Subscribed topics
void printSubscribedTopics(void)
{
    uint8_t i = 0;
    char str[MAX_SUB_CHARS];

    sendUart0String("  List of Subscribed Topics:\r\n");
    while(i < MAX_TABLE_SIZE)
    {
        if(topics[i].validBit)
        {
            sprintf(str, "    %s", topics[i].subs);
            sendUart0String(str);
            sendUart0String("\r\n");
        }
        i++;
    }
}

// Process shell commands here
void shellCommands(USER_DATA* userInput, uint8_t packet[])
{
    char token[MAX_CHARS + 1];

    /* Start of CLI Commands */
    if(isCommand(&userInput, "dhcp", 2))
    {
        getFieldString(&userInput, token, 1);

        if(strcmp(token, "on") == 0) // Enables DHCP mode and stores the mode persistently in EEPROM
        {
            etherEnableDhcpMode();
            writeEeprom(0x0010, (uint32_t)dhcpEnabled); // Store for INIT-REBOOT state
            (*dhcpLookup(nextDhcpState = INIT, DHCPDISCOVERY_EVENT))(packet);
        }
        else if(strcmp(token, "off") == 0) // Disables DHCP mode and stores the mode persistently in EEPROM
        {

            resetAllTimers();                // Turn off all clocks
            writeEeprom(0x0010, 0xFFFFFFFF); // Erase DHCP Mode in EEPROM
            setStaticNetworkAddresses();     // Update ifconfig
            etherDisableDhcpMode();
            (*dhcpLookup(NONE, NO_EVENT))(packet); // Send DHCPRELEASE
            sendArpAnnouncement(packet);           // Send ARP announcement to update network of IP address in use
        }
        else if(strcmp(token, "refresh") == 0) // Refresh Current IP address (if in DHCP mode)
        {
            if(dhcpEnabled)
            {
                renewRequest = true;
                rebindRequest = releaseRequest = false;
                dhcpRequestType = 2;
            }
        }
        else if(strcmp(token, "release") == 0) // Release Current IP address (if in DHCP mode)
        {
            if(dhcpEnabled)
            {
                resetAllTimers();
                releaseRequest = true;
                renewRequest = rebindRequest = false;
                dhcpRequestType = 0;
            }
        }
    }
    else if(isCommand(&userInput, "set", 6))
    {
        uint8_t add[4];

        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 1);

        // Get Network Address
        add[0] = getFieldInteger(&userInput, 2);
        add[1] = getFieldInteger(&userInput, 3);
        add[2] = getFieldInteger(&userInput, 4);
        add[3] = getFieldInteger(&userInput, 5);

        if(!dhcpEnabled && strcmp(token, "ip") == 0)      // Set Internet Protocol address
        {
            etherSetIpAddress(add[0], add[1], add[2], add[3]);
            storeAddressEeprom(add, 0x0011, 4);
        }
        else if(!dhcpEnabled && strcmp(token, "gw") == 0) // Set Gateway address
        {
            etherSetIpGatewayAddress(add[0], add[1], add[2], add[3]);
            storeAddressEeprom(add, 0x0012, 4);
        }
        else if(!dhcpEnabled && strcmp(token, "dns") == 0) // Set Domain Name System address
        {
            setDnsAddress(add[0], add[1], add[2], add[3]);
            storeAddressEeprom(add, 0x0013, 4);
        }
        else if(!dhcpEnabled && strcmp(token, "sn") == 0) // Set Sub-net Mask
        {
            etherSetIpSubnetMask(add[0], add[1], add[2], add[3]);
            storeAddressEeprom(add, 0x0014, 4);
        }
        else if(strcmp(token, "mqtt") == 0)              // Set Sub-net Mask
        {
            setMqttAddress(add[0], add[1], add[2], add[3]);
            storeAddressEeprom(add, 0x0015, 4);
        }
    }
    else if(isCommand(&userInput, "ifconfig", 1))
    {
        // userInput->fieldCount = 0;

        // displays current MAC, IP, GW, SN, DNS, and DHCP mode
        displayIfconfigInfo();
    }
    else if(isCommand(&userInput, "publish", 3))
    {
        char str[MAX_CHARS + 1];

        // Retrieve network configuration parameter
        getFieldString(&userInput, str, 1);

        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 2);

        // MQTT Publish Packet
        mqttPublish(packet, 0x5018, str, token);
    }
    else if(isCommand(&userInput, "subscribe", 2))
    {
        uint8_t index;

        // Copy MQTT Topic to subscribe to
        getFieldString(&userInput, token, 1);

        // Find empty slot in subscribed to table
        index = findEmptySlot();

        // Copy Subscription to Table
        strcpy(topics[index].subs, token);
        topics[index].validBit = true;

        // Send MQTT Subscribe Packet
        mqttSubscribe(packet, 0x5018, token);
    }
    else if(isCommand(&userInput, "unsubscribe", 2))
    {
        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 1);

        // Remove Topic from Subscription Table
        createEmptySlot(token);

        // Send MQTT Unsubscribe Packet
        mqttUnsubscribe(packet, 0x5018, token);
    }
    else if(isCommand(&userInput, "connect", 1))
    {
        // Mark sendMqttConnect as TRUE to initiate sending of MQTT Connect Packet
        sendMqttConnect = true;
    }
    else if(isCommand(&userInput, "disconnect", 1))
    {
        // Send MQTT Disconnect Packet
        mqttDisconnectMessage(packet, 0x5018);

        // Stop Ping Request Timer
        stopTimer(mqttPing);
    }
    else if(isCommand(&userInput, "help", 1))
    {
        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 1);

        if(strcmp(token, "inputs") == 0)
        {
            printHelpInputs(); // Print help Inputs
        }
        else if(strcmp(token, "outputs") == 0)
        {
            printHelpOututs(); // Print help Outputs
        }
        else if(strcmp(token, "subs") == 0)
        {
            printSubscribedTopics(); // Print Subscribed Topics
        }
    }
    else if(isCommand(&userInput, "reboot", 1))
    {
        // Ensure no Read or Writes to EEPROM are occuring
        while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING);

        rebootFlag = true;
    }
    else if(isCommand(&userInput, "reset", 1))
    {
        writeEeprom(0x0015, 0xFFFFFFFF); // Erase DHCP Mode in EEPROM
    }
}
