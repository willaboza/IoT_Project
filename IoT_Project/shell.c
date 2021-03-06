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
#include "shell.h"
#include "ethernet.h"
#include "uart0.h"
#include "eeprom.h"
#include "dhcp.h"
#include "reboot.h"
#include "timers.h"
#include "tcp.h"
#include "mqtt.h"

MQTT_DATA mqttInfo = {.delimeter = true,
                      .endOfString = false,
                      .fieldCount = 0,
                      .topicStartPosition = 0,
                      .msgLength = 0,
                      .topicLength = 0,
};

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

    if('a' <= c && c <= 'z' || c == '/') // Verify is character is an alpha (case sensitive)
    {
        if(data->delimeter)
        {
            data->fieldPosition[fieldIndex] = count;
            data->fieldType[fieldIndex] = 'A';
            data->fieldCount += 1;
            data->delimeter = false;
        }
    }
    else if('0' <= c && c <= '9') //Code executes for numerics same as alpha
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

// Function to Return a Token as a String
void getMQTTString(MQTT_DATA** data, uint8_t packet[], char str1[], uint8_t fieldNumber)
{
    uint8_t offset, index = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Copy characters for return
    for(offset = (*data)->fieldPosition[fieldNumber]; offset < (*data)->msgLength; offset++, index++)
    {
        str1[index] = mqtt->data[offset];
    }

    // Add NULL to terminate string
    str1[index] = '\0';
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

// Function to concatenate user input for MQTT payload
char* concatPayload(char str1[], char str2[], uint8_t index)
{
    char       *s1 = str1;
    const char *s2 = str2;

    while (*s1) s1++;        // Find end of string

    if(index != 2)       // APPEND space before next word
        (*s1++ = ' ');

    while ((*s1++ = *s2++)); // Append 2nd string

    return str1;
}

// Function to process incoming MQTT Messages
void processMqttMessage(MQTT_DATA* data, uint8_t packet[])
{
    char c;
    uint8_t fieldIndex;
    uint16_t payloadLength, i;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    mqttInfo.msgLength = mqtt->packetLength; // Get length of packet

    i = mqttInfo.topicLength = 0;
    mqttInfo.topicLength |= mqtt->data[i++] << 8; // Topic Length MSB
    mqttInfo.topicLength |= mqtt->data[i++];      // Topic Length LSB

    mqttInfo.topicStartPosition = i; // Don't need topicStartPosition

    payloadLength = mqtt->packetLength - mqttInfo.topicLength - 2 - i; // Get payload Length

    // Get current Index for field arrays
    fieldIndex = data->fieldCount = 0;
    data->delimeter = true;

    // Process Topic of PUBLISH Packet
    while(i < mqttInfo.topicLength)
    {
        c = mqtt->data[i++];

        if('a' <= c && c <= 'z' || 'A' <= c && c <= 'Z') // Verify is character is an alpha (case sensitive)
        {
            if(data->delimeter)
            {
                data->fieldPosition[fieldIndex] = i - 1;
                data->fieldType[fieldIndex++] = 'A';
                data->fieldCount += 1;
                data->delimeter = false;
            }
        }
        else if('0' <= c && c <= '9') //Code executes for numerics same as alpha
        {
            if(data->delimeter)
            {
                data->fieldPosition[fieldIndex] = i - 1;
                data->fieldType[fieldIndex++] = 'N';
                data->fieldCount += 1;
                data->delimeter = false;
            }
        }
        else if(c == '#') //Code executes # as wild-card
        {
            if(data->delimeter)
            {
                data->fieldPosition[fieldIndex] = i - 1;
                data->fieldType[fieldIndex++] = 'W';
                data->fieldCount += 1;
                data->delimeter = false;
            }
        }
        else // Insert NULL('\0') into character array if NON-alphanumeric character detected
        {
            data->delimeter = true;
        }
    }

    data->fieldPosition[fieldIndex] = mqttInfo.topicStartPosition + mqttInfo.topicLength + 2;
/*
    // If QoS Level Set then Look for Packet Identifier
    if((mqtt->control & 0x06) == 0x02 || (mqtt->control & 0x0F) == 0x04)
    {
        mqttPacketId = mqtt->data[i++] << 8; // Packet Identifier MSB
        mqttPacketId = mqtt->data[i++];      // Packet Identifier LSB

        if((mqtt->control & 0x06) == 0x02) // QoS = 1
        {
//            sendPubackFlag = true;
        }
        else if((mqtt->control & 0x0F) == 0x04) // QoS = 2
        {
//            sendPubrecFlag = true;
        }
    }
*/
}

// Function Used to Determine if Correct Command Entered
bool isMqttCommand(MQTT_DATA** data, uint8_t packet[], const char strCommand[], uint8_t pos, uint8_t minArguments)
{
    int val;
    uint8_t c1, c2, offset, index = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    if((*data)->fieldCount < minArguments)
        return false;

    offset = (*data)->fieldPosition[pos];

    while((c1 = strCommand[index++]) != '\0')
    {
        c2 = mqtt->data[offset++];
        val = c1 - c2;

        if(val != 0 || c2 == 0)
            return false;
    }

    return true;
}

// Function to Print Current Subscribed topics
void printSubscribedTopics(void)
{
    uint8_t i = 0;
    char str[MQTT_MAX_SUB_CHARS];

    sendUart0String("  List of Subscribed Topics:\r\n");
    while(i < MQTT_MAX_TABLE_SIZE)
    {
        if(topics[i].validBit)
        {
            sprintf(str, "    %s\r\n", topics[i].subs);
            sendUart0String(str);
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
                (*dhcpLookup(nextDhcpState = BOUND, DHCPREQUEST_EVENT))(packet);
        }
        else if(strcmp(token, "release") == 0) // Release Current IP address (if in DHCP mode)
        {
            if(dhcpEnabled)
            {
                resetAllTimers();
                (*dhcpLookup(NONE, NO_EVENT))(packet); // Send DHCPRELEASE
                startOneShotTimer(waitTimer, 2);
            }
        }
    }
    else if(isCommand(&userInput, "set", 6))
    {
        uint8_t i, size, add[6];

        size = userInput->fieldCount - 2;

        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 1);

        // Get Network Address
        for(i = 0; i < size; i++)
        {
            add[i] = getFieldInteger(&userInput, (i + 2));
        }

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
        else if(strcmp(token, "mqtt") == 0) // Set Sub-net Mask
        {
            if(size == 4) // Set MQTT Broker IP address
            {
                setMqttAddress(add[0], add[1], add[2], add[3]);
                storeAddressEeprom(add, 0x0001, 4);
            }
            else if(size == 6) // Set MQTT Broker Mac Address
            {
                setAddressInfo(mqttMacAddress, add, 6);
                storeAddressEeprom(add, 0x0002, 6);
            }
        }
    }
    else if(isCommand(&userInput, "ifconfig", 1)) // displays current MAC, IP, GW, SN, DNS, and DHCP mode
    {
        displayIfconfigInfo();
    }
    else if(isCommand(&userInput, "publish", 3))
    {
        uint8_t i;
        char str[MAX_CHARS + 1];

        // Get MQTT payload to publish
        for(i = 2; i < userInput->fieldCount; i++)
        {
            getFieldString(&userInput, str, i);

            concatPayload(token, str, i);
        }

        // Get MQTT topic to publish
        getFieldString(&userInput, str, 1);

        // send MQTT Publish Packet
        sendMqttPublish(packet, 0x5018, str, token);
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
        tcb.prevSeqNum = tcb.prevAckNum = tcb.currentAckNum = tcb.currentAckNum = 0;

        // Change TCP state to CLOSED
        nextTcpState = CLOSED;

        // Send TCP SYN message to initiate connection with MQTT broker
        sendTcpMessage(packet, NOPE);
    }
    else if(isCommand(&userInput, "disconnect", 1))
    {
        // Stop Ping Request Timer
        stopTimer(mqttPingTimerExpired);

        // Change TCP State to CLOSING
        nextTcpState = CLOSING;

        // Send MQTT Disconnect Packet
        sendMqttDisconnectMessage(packet, 0x5018);

        // Send FIN+ACK to MQTT broker to begin closing of connection
        //sendTcpMessage(packet, FIN_ACK);
    }
    else if(isCommand(&userInput, "help", 1))
    {
        // Retrieve network configuration parameter
        getFieldString(&userInput, token, 1);

        if(strcmp(token, "inputs") == 0)
            printHelpInputs(); // Print help Inputs
        else if(strcmp(token, "outputs") == 0)
            printHelpOututs(); // Print help Outputs
        else if(strcmp(token, "subs") == 0)
            printSubscribedTopics(); // Print Subscribed Topics
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

// Start of IFTTT Rules Table
void ifttRulesTable(MQTT_DATA* mqttInput, uint8_t packet[])
{
    char buffer[50];

    if(isMqttCommand(&mqttInput, packet, "env", 0, 2))
    {
        if(isMqttCommand(&mqttInput, packet, "temp", 1, 2))
        {

            getMQTTString(&mqttInput, packet, buffer, 2);

            // Send Published Temperature to UART
            //sprintf(str, "Degrees Celsius : %u", temp);
            sendUart0String(buffer);
            sendUart0String("\r\n");

            // Update env/temp with more recent value and then MQTT Publish Packet
            // sprintf(str, "%s", "env/temp");
            // sprintf(buffer, "%u", instantTemp());
            // mqttPublish(data, 0x5018, str, buffer);
        }
        else if(isMqttCommand(&mqttInput, packet, "led", 1, 2)) // Part of topic
        {
            if(isMqttCommand(&mqttInput, packet, "green", 2, 2)) // Part of topic
            {
                getMQTTString(&mqttInput, packet, buffer, 3);

                if(strcmp(buffer, "on") == 0) // Part of payload
                {
                    setPinValue(GREEN_LED, 1); // Green LED ON
                    sendUart0String("  GREEN LED ON\r\n");
                }
                else if(strcmp(buffer, "off") == 0) // Part of payload
                {
                    setPinValue(GREEN_LED, 0); // Green LED OFF
                    sendUart0String("  GREEN LED OFF\r\n");
                }
            }
            else if(isMqttCommand(&mqttInput, packet, "red", 2, 2))
            {
                getMQTTString(&mqttInput, packet, buffer, 3);
                if(strcmp(buffer, "on") == 0)
                {
                    setPinValue(RED_LED, 1); // RedLED ON
                    sendUart0String("  RED LED ON\r\n");
                }
                else if(strcmp(buffer, "off") == 0)
                {
                    setPinValue(RED_LED, 0); // RED LED OFF
                    sendUart0String("  RED LED OFF\r\n");
                }
            }
            else if(isMqttCommand(&mqttInput, packet, "blue", 2, 2))
            {
                getMQTTString(&mqttInput, packet, buffer, 3);
                if(strcmp(buffer, "on") == 0) // Payload
                {
                    setPinValue(BLUE_LED, 1); // Blue LED ON
                    sendUart0String("  BLUE LED ON\r\n");
                }
                else if(strcmp(buffer, "off") == 0) // Payload
                {
                    setPinValue(BLUE_LED, 0); // Blue LED OFF
                    sendUart0String("  BLUE LED OFF\r\n");
                }
            }
        }
    }
}
