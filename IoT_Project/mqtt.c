// mqtt.c
// William Bozarth
// Created on: April 14, 2020

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
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "mqtt.h"
#include "ethernet.h"
#include "shell.h"
#include "tcp.h"
#include "uart0.h"
#include "timers.h"

mqttTopics topics[MQTT_MAX_TABLE_SIZE] = {0};
uint8_t mqttIpAddress[MQTT_ADD_LENGTH] = {0};
uint8_t mqttMacAddress[MQTT_HW_ADD_LENGTH] = {0};
uint8_t mqttMsgType = 0;
uint16_t mqttSrcPort = 54000;
uint16_t mqttPacketId = 0;

// Set MQTT Address
void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3)
{
    mqttIpAddress[0] = mqtt0;
    mqttIpAddress[1] = mqtt1;
    mqttIpAddress[2] = mqtt2;
    mqttIpAddress[3] = mqtt3;
}

// Get MQTT Address
void getMqttAddress(uint8_t mqtt[])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        mqtt[i] = mqttIpAddress[i];
}

// Function finds 1st slot with validBit field set to false and returns the index value to it.
uint8_t findEmptySlot(void)
{
    uint8_t i;

    i = 0;
    while(i < MQTT_MAX_TABLE_SIZE)
    {
        if(!(topics[i].validBit))
            return i;
        i++;
    }
    return 0;
}

// Function to Create Empty Slot in Subscriptions Table for re-use
void createEmptySlot(char info[])
{
    uint8_t i = 0;

    while(i < MQTT_MAX_TABLE_SIZE)
    {
        if(strcmp(topics[i].subs, info) == 0)
        {
            topics[i].validBit = false;
            return;
        }
        i++;
    }
}

// Determines whether packet is MQTT
bool isMqttMessage(uint8_t packet[])
{
    uint16_t index;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    // Find where data begins for TCP Header and then point the mqttFrame to that index
    index = (((htons(tcp->dataCtrlFields) & 0xF000) >> 12) - 5) * 4;
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data[index];

    // If Source Port = 1883 it is a MQTT packet.
    // Further filter for only MQTT Publish packets received from broker
    if(htons(tcp->sourcePort) == 1883 && (mqtt->control & 0xF0) == 48)
        return true;

    return false;
}

// Returns MQTT packet type
uint8_t getMqttMsgType(uint8_t packet[])
{
    uint16_t index;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    // Find where data begins for TCP Header and then point the mqttFrame to that index
    index = (((htons(tcp->dataCtrlFields) & 0xF000) >> 12) - 5) * 4;
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data[index];

    return (mqtt->control & 0xF0);
}

void mqttMessageEstablished(void)
{
    //uint8_t packet[MAX_PACKET_SIZE];

    sendMqttConnectMessage(data, 0x5018); // Flag = PSH + ACK

    startPeriodicTimer(mqttPingTimerExpired, (MQTT_KEEP_ALIVE_TIME * MULT_FACTOR));

    stopTimer(mqttMessageEstablished);
}

void mqttPingTimerExpired(void)
{
    //uint8_t packet[MAX_PACKET_SIZE];

    sendMqttPingRequest(data, 0x5018);
}
/*
// Algorithm for encoding a non-negative integer into the variable length encoding scheme
uint32_t encodeRemainingLength(uint8_t packet[], uint32_t )
{
    uint8_t encodedByte;
    uint32_t value;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    while(value > 0)
    {

    }
}

// Algorithm for decoding the Remaining Length field
uint32_t decodeRemainingLength(uint8_t packet[])
{
    uint8_t encodedByte;
    uint32_t multiplier = 1, value = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;


}
*/

// Connect Variable Header Contains: Protocol Name, Protocol Level, Connect Flags, Keep Alive, and Properties
void sendMqttConnectMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = mqttMacAddress[i];
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;               // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;                  // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->seqNum = tcb.currentSeqNum;
    tcp->ackNum = tcb.currentAckNum;
    tcp->dataCtrlFields = htons(flags);

    // Connect Packet Type = 1, Flags = 0
    mqtt->control = 0x10;

    // Connect Variable Header Contains:
    //      Protocol Name
    //      Protocol Level
    //      Connect Flags
    //      Keep Alive
    //      Properties

    // Protocol Name
    i = 0;
    mqtt->data[i++] = 0x00; // Length MSB
    mqtt->data[i++] = 0x04; // Length LSB
    mqtt->data[i++] = 'M';
    mqtt->data[i++] = 'Q';
    mqtt->data[i++] = 'T';
    mqtt->data[i++] = 'T';

    // Protocol Level
    mqtt->data[i++] = 0x04;

    // Connect Flags:
    //      |       7       |       6       |      5      |  4  | 3  |     2     |       1       |    0     |
    //      | User Name Flag| Password Flag | Will Retain | Will QoS | Will Flag | Clean Session | Reserved |
    mqtt->data[i++] = 0x02;

    // Keep Alive = 60
    mqtt->data[i++] = 0x00; // MSB
    mqtt->data[i++] = MQTT_KEEP_ALIVE_TIME; // LSB

    // Client ID Length
    mqtt->data[i++] = 0x00;
    mqtt->data[i++] = 0x08;

    // Client ID
    mqtt->data[i++] = 'm';
    mqtt->data[i++] = 'o';
    mqtt->data[i++] = 'T';
    mqtt->data[i++] = 'h';;
    mqtt->data[i++] = 'R';
    mqtt->data[i++] = 'a';
    mqtt->data[i++] = '-';
    mqtt->data[i++] = '1';

    mqtt->packetLength = i; // MQTT Message Length

    tcpSize = (sizeof(tcpFrame) + (i + 2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// MQTT Connect+Ack Message
void sendMqttDisconnectMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = mqttMacAddress[i];
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;            // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = htons(0x4000);   // Don't Fragment Flag Set
    ip->ttl            = 30;              // Time-to-Live in seconds
    ip->protocol       = 6;               // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);     // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;               // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(flags);

    if(tcb.currentSeqNum != tcb.prevSeqNum)
    {
        tcp->seqNum = tcb.prevSeqNum = tcb.currentSeqNum;
        tcp->ackNum = tcb.prevAckNum = tcb.currentAckNum;
    }
    else
    {
        tmp32 = htons32(tcb.prevSeqNum) + 1;
        tcp->seqNum = tcb.currentSeqNum = tcb.prevSeqNum = htons32(tmp32);
        tcp->ackNum = tcb.prevAckNum = tcb.currentAckNum;
    }

    i = 0;
    // DISCONNECT
    mqtt->control = 0xE0;

    // MQTT Message Length
    mqtt->packetLength = 0; // No Variable Header or Payload for MQTT Disconnect

    tcpSize = (sizeof(tcpFrame) + (i + 2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

void sendMqttPingRequest(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = mqttMacAddress[i];
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;               // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;                  // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(flags);
    tcp->seqNum = tcb.currentSeqNum;
    tcp->ackNum = tcb.currentAckNum;
    /*
    if(tcb.currentSeqNum != tcb.prevSeqNum)
    {
        //tcb.prevSeqNum = tcb.currentSeqNum;
        //tcb.prevAckNum = tcb.currentAckNum;
        tcp->seqNum = tcb.currentSeqNum;
        tcp->ackNum = tcb.currentAckNum;
    }
    else
    {
        tmp32 = htons32(tcb.prevSeqNum) + 1;
        tcb.prevSeqNum = tcb.currentSeqNum;
        tcb.prevAckNum = tcb.currentAckNum;
        tcp->seqNum = tcb.currentSeqNum = htons32(tmp32);
        tcp->ackNum = tcb.currentAckNum;
    }
    */
    i = 0;

    // PING REQUEST
    mqtt->control = 0xC0;

    // MQTT Message Length
    mqtt->packetLength = 0; // No Variable Header or Payload for MQTT Ping Request

    tcpSize = (sizeof(tcpFrame) + (i + 2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// MQTT Connect+Ack Message
void sendMqttPublish(uint8_t packet[], uint16_t flags, char topic[], char data[])
{
    uint8_t i = 0, k;
    uint16_t tcpSize = 0, tmp16 = 0, length, packetId;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = mqttMacAddress[i];
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize       = 0x45;                // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;                  // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = tcb.currentSeqNum;
    tcp->ackNum = tcb.currentAckNum;

    // PUBLISH Packet
    mqtt->control = 0x31; // RETAIN Flag is set

    // TOPIC NAME
    i = 2;
    k = 0;
    while(topic[k] != '\0')
    {
        mqtt->data[i++] = topic[k++];
    }
    // Get TOPIC NAME Length (2 bytes in length)
    length = (i - 2);
    mqtt->data[0] = length >> 8; // Length MSB
    mqtt->data[1] = length;      // Length LSB

    if((mqtt->control & 0x06) == 0x02 || (mqtt->control & 0x0F) == 0x04) // If QoS Level Set then Look for Packet Identifier
    {
        // Packet Identifier
        packetId = random32();
        mqtt->data[i++] = packetId >> 8; // ID MSB
        mqtt->data[i++] = packetId;      // ID LSB
    }

    // Payload
    k = 0;
    while(data[k] != '\0')
    {
        mqtt->data[i++] = data[k++];
    }

    // MQTT Message Length
    mqtt->packetLength = i; // No Variable Header or Payload for MQTT Disconnect

    tcpSize = (sizeof(tcpFrame) + (i+2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// Function for Sending MQTT PUBACK Message
void mqttPubAckRec(uint8_t packet[], uint8_t type, uint16_t flags, uint16_t packetId)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;               // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = htons(0x4000);      // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(flags);
    tcp->seqNum = tcb.currentSeqNum;
    tcp->ackNum = tcb.currentAckNum;

    if(type == 4)
    {
        // PUBACK Packet
        mqtt->control = 0x40;
    }
    else if(type == 5)
    {
        // PUBREC Packet
        mqtt->control = 0x50;
    }

    // MQTT Message Length
    mqtt->packetLength = 2;

    i = 0;
    // Packet Identifer
    mqtt->data[i++] = packetId >> 8; // ID MSB
    mqtt->data[i++] = packetId;      // ID LSB

    tcpSize = (sizeof(tcpFrame) + (i + 2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// Function for MQTT Subscribe
void mqttSubscribe(uint8_t packet[], uint16_t flags, char topic[])
{
    uint8_t i = 0, k, offset;
    uint16_t tcpSize = 0, tmp16 = 0, length, packetId;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;               // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = htons(0x4000);      // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = tcb.currentSeqNum;
    tcp->ackNum = tcb.currentAckNum;
    tcb.prevSeqNum += 1;

    // SUBSCRIBE Packet
    mqtt->control = 0x82;

    // Packet Identifier
    i = 0;
    packetId = random32();
    mqtt->data[i++] = packetId >> 8; // ID MSB
    mqtt->data[i++] = packetId;      // ID LSB

    // TOPIC NAME
    offset = i;
    i += 2;
    k = 0;
    while(topic[k] != '\0')
    {
        mqtt->data[i++] = topic[k++];
    }

    // Get TOPIC NAME Length (2 bytes in length)
    length = (i - offset - 2);
    mqtt->data[offset++] = length >> 8; // Length MSB
    mqtt->data[offset++] = length;      // Length LSB

    // Topic QoS
    mqtt->data[i++] = 0x01;

    // MQTT Message Length
    mqtt->packetLength = i; // No Variable Header or Payload for MQTT Disconnect

    tcpSize = (sizeof(tcpFrame) + (i+2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// Function for MQTT Unsubscribe Packet
void mqttUnsubscribe(uint8_t packet[], uint16_t flags, char topic[])
{
    uint8_t i = 0, k, offset;
    uint16_t tcpSize = 0, tmp16 = 0, length, packetId;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = mqttIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize       = 0x45;                // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = htons(0x4000);      // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE;       // Time-to-Live in seconds
    ip->protocol       = 6;                  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = htons(1883);        // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;                  // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;                  // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = tcb.prevSeqNum;
    tcp->ackNum = tcb.prevAckNum;
    tcb.prevSeqNum+=1;

    // UNSUBSCRIBE Packet
    mqtt->control = 0xA2;

    // Packet Identifier
    i = 0;
    packetId = random32();
    mqtt->data[i++] = packetId >> 8; // ID MSB
    mqtt->data[i++] = packetId;      // ID LSB

    // TOPIC NAME
    offset = i;
    i += 2;
    k = 0;
    while(topic[k] != '\0')
    {
        mqtt->data[i++] = topic[k++];
    }

    // Get TOPIC NAME Length (2 bytes in length)
    length = (i - offset - 2);
    mqtt->data[offset++] = length >> 8; // Length MSB
    mqtt->data[offset++] = length;      // Length LSB

    // MQTT Message Length
    mqtt->packetLength = i; // No Variable Header or Payload for MQTT Disconnect

    tcpSize = (sizeof(tcpFrame) + (i + 2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    tmp16 = 0;
    etherSumWords(ip->sourceIp, 8);
    sum += ((tmp16 & 0xFF) << 8);
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}
