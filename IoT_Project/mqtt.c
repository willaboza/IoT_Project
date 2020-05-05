/*
 * mqtt.c
 *
 *  Created on: Apr 14, 2020
 *      Author: William Bozarth
 */

#include "mqtt.h"

uint8_t ipMqttAddress[IP_ADD_LENGTH]    = {192,168,1,67};
uint8_t mqttBrokerMac[HW_ADD_LENGTH]    = {0x04,0xD3,0xB0,0x7F,0x7E};
uint8_t mqttMsgType = 0;
uint16_t mqttSrcPort = 49100;
uint16_t packetId = 0;

bool mqttMsgFlag = false;
bool sendMqttConnect = false;
bool sendPubackFlag = false;
bool sendPubrecFlag = false;
bool pubMessageReceived = false;

subscribedTopics topics[MAX_TABLE_SIZE] = {0};

// Set DNS Address
void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3)
{
    ipMqttAddress[0] = mqtt0;
    ipMqttAddress[1] = mqtt1;
    ipMqttAddress[2] = mqtt2;
    ipMqttAddress[3] = mqtt3;
}

// Get DNS Address
void getMqttAddress(uint8_t mqtt[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        mqtt[i] = ipMqttAddress[i];
}

// Function finds 1st slot with validBit field set to false and returns the index value to it.
uint8_t findEmptySlot()
{
    uint8_t i, tmp8;
    bool ok = true;
    i = 0;
    while((i < MAX_TABLE_SIZE) && ok)
    {
        if(!(topics[i].validBit))
        {
            tmp8 = i;
            ok = false;
        }
        i++;
    }
    return tmp8;
}

// Function to Create Empty Slot in Subscriptions Table for re-use
void createEmptySlot(char info[])
{
    uint8_t i = 0;
    bool ok = true;

    i = 0;
    while((i < MAX_TABLE_SIZE) && ok)
    {
        if(strcmp(topics[i].subs, info) == 0)
        {
            topics[i].validBit = false;
            ok = true;
        }
        i++;
    }
}

// Determines whether packet is MQTT
bool mqttMessage(uint8_t packet[])
{
    bool ok= false;
    uint16_t tmp16;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    tmp16 = htons(tcp->dataCtrlFields);

    // Find where data begins for TCP Header and then point the mqttFrame to that index
    tmp16 = (((tmp16 & 0xF000) >> 12) - 5) * 4;
    mqttFrame *mqtt   = (mqttFrame*)&tcp->data[tmp16];

    // If Source Port = 1883 it is a MQTT packet.
    // Further filter for only MQTT Publish packets received from broker
    tmp16 = htons(tcp->sourcePort);
    if((tmp16 == 1883) && (mqtt->control & 0xF0) == 48)
    {
        ok = true;
    }

    return ok;
}

// Connect Variable Header Contains: Protocol Name, Protocol Level, Connect Flags, Keep Alive, and Properties
void mqttConnectMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;
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
    mqtt->data[i++] = KEEP_ALIVE_TIME; // LSB

    // Client ID Length
    mqtt->data[i++] = 0x00;
    mqtt->data[i++] = 0x04;

    // Client ID
    mqtt->data[i++] = 'm';
    mqtt->data[i++] = 'o';
    mqtt->data[i++] = 't';
    mqtt->data[i++] = 'h';

    mqtt->packetLength = i; // MQTT Message Length

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

// MQTT Connect+Ack Message
void mqttDisconnectMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;

    i = 0;
    // DISCONNECT
    mqtt->control = 0xE0;

    // MQTT Message Length
    mqtt->packetLength = 0; // No Variable Header or Payload for MQTT Disconnect

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

void mqttPingRequest(uint8_t packet[], uint16_t flags)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;
    prevSeqNum += 1;

    i = 0;
    // PING REQUEST
    mqtt->control = 0xC0;

    // MQTT Message Length
    mqtt->packetLength = 0; // No Variable Header or Payload for MQTT Ping Request

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

// MQTT Connect+Ack Message
void mqttPublish(uint8_t packet[], uint16_t flags, char topic[], char data[])
{
    uint8_t i = 0, k;
    uint16_t tcpSize = 0, tmp16 = 0, length, packetId;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;

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
void mqttPubAckRec(uint8_t packet[], uint16_t flags, uint8_t type)
{
    uint8_t i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;

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

// Function for MQTT Subscribe
void mqttSubscribe(uint8_t packet[], uint16_t flags, char topic[])
{
    uint8_t i = 0, k, offset;
    uint16_t tcpSize = 0, tmp16 = 0, length, packetId;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;
    prevSeqNum += 1;

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
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
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
        ip->destIp[i]   = ipMqttAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
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
    tcp->dataCtrlFields = htons(0x5018);
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;
    prevSeqNum+=1;

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
