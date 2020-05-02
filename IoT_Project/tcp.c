/*
 * tcp.c
 *
 *  Created on: Mar 8, 2020
 *      Author: willi
 *
 *   Resources:
 */

#include "tcp.h"

bool listenState = true;
bool establishedState = false;
bool closeState = false;
uint32_t prevSeqNum = 0;
uint32_t prevAckNum = 0;

// Determines if Packet recieved is TCP
bool etherIsTcp(uint8_t packet[])
{
    bool ok = false;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;

    if(ip->protocol == 6)
    {
        ok = true;
    }

    return ok;
}

//
bool checkForDuplicates(uint8_t packet[])
{
    bool ok = false;
    uint32_t tmp32;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    tmp32 = htons32(tcp->seqNum);

    if(prevSeqNum != tmp32)
    {
        ok = true;
    }

    return ok;
}

// Function to find TCP flags
uint8_t etherIsTcpMsgType(uint8_t packet[])
{
    uint8_t num = 0;
    uint16_t tmp16;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    tmp16 = htons(tcp->dataCtrlFields);

    if((tmp16 & 0xFFF) == 0x002)      // SYN
    {
        num = 1;
    }
    else if((tmp16 & 0xFFF) == 0x010) // ACK
    {
        num = 2;
    }
    else if((tmp16 & 0xFFF) == 0x18) // PSH
    {
        num = 3;
    }
    else if((tmp16 & 0xFFF) == 0x011) // FIN+ACK
    {
        num = 4;
        establishedState = false;
        closeState = true;
    }
    else if((tmp16 & 0xFFF) == 0x004) // RST
    {
        num = 5;
    }
    else if((tmp16 & 0xFFF) == 0x012) // SYN+ACK
    {
        num = 6;
    }

    return num;
}

// Function used to send TCP messages
void sendTcpMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t tmp8 = 0, i = 0;
    uint16_t tcpSize = 0, tmp16 = 0, tmpSrcPrt;
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
        tmp8 = ip->destIp[i];
        ip->destIp[i] = ip->sourceIp[i];
        ip->sourceIp[i] = tmp8;
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
    tcp->destPort      = tmpSrcPrt; // Destination Port is
    tcp->sourcePort    = tmp16;
    tcp->checksum      = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;               // Not used in this class
    tcp->window        = htons(1024);

    tmp16 = htons(tcp->dataCtrlFields);
    i = 0;

    // If SYN flag = 1, then this is the initial sequence #.
    // The sequence # of actual 1st data byte and the acknowledge #
    // in the corresponding ACK is acknowledge # = sequence # + 1.
    if((tmp16 & 0xFFF) == 0x002) // TCP SYN
    {
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->ackNum = htons32(tmp32);
        tcp->seqNum = 0;

        tcp->data[i++] = 0x02; // Kind = 2, Maximum Segment Size
        tcp->data[i++] = 0x04; // Length = 4
        tcp->data[i++] = 0x04; // 1st byte of MSS
        tcp->data[i++] = 0x00; // 2nd byte of MSS
    }
    else if((tmp16 & 0xFFF) == 0x012) // TCP SYN+ACK
    {
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
    }
    else if((tmp16 & 0xFFF) == 0x018) // TCP PSH+ACK
    {
        // Size of TCP Data should be ip->length - size of IP Header - size of TCP Header
        tmp16 = (htons(ip->length) - ((ip->revSize & 0xF) * 4) - (((htons(tcp->dataCtrlFields) & 0xF000) * 4) >> 12));

        tmp32 = htons32(tcp->seqNum) + tmp16;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
    }
    else if((tmp16 & 0xFFF) == 0x011) // FIN+ACK
    {
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
    }

    prevSeqNum = tcp->seqNum;
    prevAckNum = tcp->ackNum;

    tcpSize = (sizeof(tcpFrame) + i); // Size of Options

    tcp->dataCtrlFields = htons(flags);

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

void getTcpData(uint8_t packet[])
{
    uint16_t i, tmp16;
    char c;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    // Size of TCP Data should be ip->length - size of IP Header - size of TCP Header
    tmp16 = (htons(ip->length) - ((ip->revSize & 0xF) * 4) - (((htons(tcp->dataCtrlFields) & 0xF000) * 4) >> 12));


    // Output TCP Client Data to Terminal
    sendUart0String("  ");
    sendUart0String(tcp->data[0]);
    sendUart0String("\r\n");
}
