/*
 * tcp.c
 *
 *  Created on: Mar 8, 2020
 *      Author: willi
 *
 *   Resources:
 */

#include "tcp.h"

bool openState = true;
bool establishedState = false;
bool closeState = false;

//
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
uint8_t etherIsTcpMsgType(uint8_t packet[])
{
    uint8_t num = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    if(htons(tcp->dataCtrlFields) & 0x2)       // SYN
    {
        num = 1;
    }
    else if(htons(tcp->dataCtrlFields) & 0x10) // ACK
    {
        num = 2;
    }
    else if(htons(tcp->dataCtrlFields) & 0x8) // PSH
    {
        num = 3;
    }
    else if(htons(tcp->dataCtrlFields) & 0x1) // FIN
    {
        num = 4;
    }

    return num;
}

// Function used to send TCP messages
void sendTcpMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t tmp8 = 0, i = 0;
    uint16_t tcpSize = 0, tmp16 = 0;
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp8 = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp8;
    }

    ether->frameType = 0x0080; // For ipv4

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
    ip->id             = (ip->id + 1);
    ip->flagsAndOffset = 0x0040;          // Don't Fragment Flag Set
    ip->ttl            = 30;              // Time-to-Live in seconds
    ip->protocol       = 6;               // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    tcp->destPort      = tcp->sourcePort; // Destination Port is
    tcp->sourcePort    = htons(6789);
    tcp->checksum      = 0;               // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;               // Not used in this class
    tcp->window        = htons(1);

    // If SYN flag = 1, then this is the initial sequence #.
    // The sequence # of actual 1st data byte and the acknowledge #
    // in the corresponding ACK is acknowledge # = sequence # + 1.
    if(htons(tcp->dataCtrlFields) & 0x2) // SYN
    {
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->ackNum = htons32(tmp32);
        tcp->seqNum = 0;
    }
    else if(htons(tcp->dataCtrlFields) & 0x10)
    {
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
    }
    else if(htons(tcp->dataCtrlFields) & 0x18)
    {
       tcp->seqNum = htons32(tcp->seqNum);
       tmp32 = tcp->seqNum + 1;
       tcp->ackNum = htons32(tcp->seqNum + 1);
       tmp32 = (sizeof(tcp) - sizeof(tcpFrame));
       tmp32 += tcp->seqNum;
       tcp->seqNum = htons32(tmp32);
    }

    tcp->dataCtrlFields = htons(flags);  // 0x5 is the size of the TCP Header in 32 bit words

    i = 0;
    /*
    tcp->data[i++] = 0x02; // Kind = 2, Maximum Segment Size
    tcp->data[i++] = 0x04; // Length = 4
    tcp->data[i++] = 0x00; // 1st byte of MSS
    tcp->data[i++] = 0x00; // 2nd byte of MSS
    tcp->data[i++] = 0x01; // 3rd byte of MSS
    tcp->data[i++] = 0x00; // 4th byte of MSS
    tcp->data[i++] = 0x00; // Kind = 0, End of Option List
    */
    tcpSize = (sizeof(tcpFrame) + i); // Size of Options

    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + tcpSize); // Adjust length of IP header
//    etherSumWords(&ip->revSize, (ip->revSize & 0xF) * 4);
//    ip->headerChecksum = getEtherChecksum();
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    // TCP pseudo-header includes 4 byte source address from IP header,
    // 4 byte destination address from IP header, 1 byte of zeros, 1 byte of protocol
    // field from IP header, and 2 bytes for TCP length (Includes both TCP header and data).
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = 0; // For Reserved Bits
    sum += (tmp16 & 0xFF) << 8;
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xFF) << 8;
    sum += tcpSize;
    etherSumWords(&tcp->dataCtrlFields, 1);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);
    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    if(etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize))
    {
        if(flags & 0x01)
        {
            putsUart0("  FIN Sent.\r\n");
        }
        else if(flags & 0x02)
        {
            putsUart0("  SYN Sent.\r\n");
        }
        else if(flags & 0x12)
        {
            putsUart0("  SYN+ACK Sent.\r\n");
        }
        else if(flags & 0x08)
        {
            putsUart0("  PSH Sent.\r\n");
        }
        else if(flags & 0x10)
        {
            putsUart0("  ACK Sent.\r\n");
        }
    }
}
