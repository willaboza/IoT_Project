/*
 * mqtt.c
 *
 *  Created on: Apr 14, 2020
 *      Author: William Bozarth
 */

#include "mqtt.h"

uint8_t ipMqttAddress[IP_ADD_LENGTH]    = {192,168,1,65};
uint8_t localHostAddress[IP_ADD_LENGTH] = {192,168,1,65};
uint8_t mqttBrokerMac[HW_ADD_LENGTH]    = {0x04,0xD3,0xB0,0x7F,0x7E};
bool mqttMsgFlag = false;
uint8_t mqttMsgType = 0;
uint16_t mqttSrcPort = 0;

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

// Determines whether packet is MQTT
bool etherIsMqttRequest(uint8_t packet[])
{
    bool ok= false;
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    if(tcp->sourcePort == 1883)
    {
        ok = true;
    }

    return ok;
}

void sendMqttTcpSyn(uint8_t packet[], uint16_t flags)
{
    uint16_t tcpSize = 0, tmp16 = 0, i = 0;
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

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
    ip->ttl            = 60;              // Time-to-Live in seconds
    ip->protocol       = 6;               // TCP = 6 (See RFC790 Assigned Internet Protocol Numbers table for list)

    mqttSrcPort = 49182;
    tcp->destPort       = htons(1883);     // Destination Port is
    tcp->sourcePort     = htons(mqttSrcPort);
    tcp->checksum       = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer  = 0;               // Not used in this class
    tcp->window         = htons(1024);
     tmp32 = random32();
    tcp->seqNum         = htons32(tmp32);
    tcp->ackNum         = 0;
    tcp->dataCtrlFields = htons(flags);

    i = 0;

    // MSS
    tcp->data[i++] = 0x02; // Kind = 2, Maximum Segment Size
    tcp->data[i++] = 0x04; // Length = 4
    tcp->data[i++] = 0x04; // 1st byte of MSS
    tcp->data[i++] = 0x00; // 2nd byte of MSS
    /*
    // Window Scale
    tcp->data[i++] = 0x01; // NOP
    tcp->data[i++] = 0x03; // Kind: Window Scale (3)
    tcp->data[i++] = 0x03; // Length
    tcp->data[i++] = 0x02; // Shift Count

    // SACK Permitted
    tcp->data[i++] = 0x01; // NOP
    tcp->data[i++] = 0x01; // NOP
    tcp->data[i++] = 0x04; // SACK permitted
    tcp->data[i++] = 0x02; // Length
    */

    tcpSize = (sizeof(tcpFrame) + i); // Size of Options

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

// Connect Variable Header Contains: Protocol Name, Protocol Level, Connect Flags, Keep Alive, and Properties
void mqttConnectMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t tmp8 = 0, i = 0;
    uint16_t tcpSize = 0, tmp16 = 0, tmpSrcPrt;
    uint32_t tmp32 = 0;

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

    tmpSrcPrt = tcp->sourcePort;
    tmp16 = tcp->destPort;
    tcp->destPort      = htons(1883);     // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;               // Not used in this class
    tcp->window        = htons(1024);
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;
    tcp->dataCtrlFields = htons(flags);

    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;

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
    mqtt->data[i++] = 0x3C; // LSB

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

    // Size of TCP Data should be ip->length - size of IP Header - size of TCP Header
    // tmp16 = (htons(ip->length) - ((ip->revSize & 0xF) * 4) - (((htons(tcp->dataCtrlFields) & 0xF000) * 4) >> 12));

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
    uint8_t tmp8 = 0, i = 0;
    uint16_t tcpSize = 0, tmp16 = 0, tmpSrcPrt;
    uint32_t tmp32 = 0;

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

    tmpSrcPrt = tcp->sourcePort;
    tmp16 = tcp->destPort;
    tcp->destPort      = htons(1883);     // Destination Port is
    tcp->sourcePort    = htons(mqttSrcPort);
    tcp->checksum      = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;               // Not used in this class
    tcp->window        = htons(1024);
    tcp->dataCtrlFields = htons(flags);
    tcp->seqNum = prevSeqNum;
    tcp->ackNum = prevAckNum;

    i = 0;
    // Connect Packet Type = 1, Flags = 0
    mqtt->control = 0xE0;

    // MQTT Message Length
    mqtt->packetLength = 0; // No Variable Header or Payload for MQTT Disconnect

    tcpSize = (sizeof(tcpFrame) + (i+2)); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
    etherCalcIpChecksum(ip);

    // Size of TCP Data should be ip->length - size of IP Header - size of TCP Header
    // tmp16 = (htons(ip->length) - ((ip->revSize & 0xF) * 4) - (((htons(tcp->dataCtrlFields) & 0xF000) * 4) >> 12));

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
