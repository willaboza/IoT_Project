// tcp.c
// William Bozarth
// Created on: March 8, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "tcp.h"
#include "ethernet.h"
#include "timers.h"
#include "mqtt.h"

transCtrlBlock tcb = {.currentAckNum = 0,
                      .currentSeqNum = 0,
                      .prevAckNum = 0,
                      .prevSeqNum = 0
};

tcpSysState nextTcpState = LISTEN;

tcpStateMachine tcpStateTransitions [] =
{
    {CLOSED,       PASSIVE_OPEN_EVENT, (_tcpCallback)setUpTcb},       //
    {LISTEN,       SYN_EVENT,          (_tcpCallback)sendTcpMessage}, //
    {SYN_SENT,     SYN_ACK_EVENT,      (_tcpCallback)sendTcpMessage}, //
    {SYN_RECEIVED, ACK_EVENT,          (_tcpCallback)tcpEstablished}, //
    {SYN_RECEIVED, SYN_EVENT,          (_tcpCallback)dupTcpMsg},      //
    {ESTABLISHED,  PSH_ACK_EVENT,      (_tcpCallback)sendTcpMessage}, //
    {ESTABLISHED,  FIN_EVENT,          (_tcpCallback)sendTcpMessage}, //
    {ESTABLISHED,  FIN_ACK_EVENT,      (_tcpCallback)sendTcpMessage}, //
    {CLOSE_WAIT,   APP_CLOSE_EVENT,    (_tcpCallback)sendTcpMessage}, //
    {CLOSING,      FIN_ACK_EVENT,      (_tcpCallback)sendTcpMessage}, //
    {LAST_ACK,     ACK_EVENT,          (_tcpCallback)tcpClose}        //
};

//
void dupTcpMsg(void){return;}

// Determines if Packet recieved is TCP
bool etherIsTcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;

    if(ip->protocol == 6)
        return true;

    return false;
}

// Function to Check For Duplicate or Re-transmissions
bool checkForDuplicates(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    if(tcb.prevSeqNum != htons32(tcp->seqNum))
        return true;

    return false;
}

// Function to find TCP flags
uint16_t etherIsTcpMsgType(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    return (uint8_t)(htons(tcp->dataCtrlFields) & 0x001F);
}

// Function to Adjust SEQ Number and ACK Number on Receiving a TCP ACK Message
void tcpAckReceived(uint8_t packet[])
{
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    tmp32 = htons32(tcp->seqNum);
    tcb.prevSeqNum = tcp->seqNum = tcp->ackNum;
    tcb.prevAckNum = tcp->ackNum = htons32(tmp32);
}

// Set-up initial conditions for Transmission Control Block
void setUpTcb(void)
{
    tcb.prevSeqNum = 0;
    tcb.prevAckNum = 0;
    tcb.currentAckNum = 0;
    tcb.currentSeqNum = 0;
    nextTcpState = LISTEN;
}

// Transition to TCP closed state
void tcpClose(void)
{
    nextTcpState = CLOSED;

    setUpTcb();

    stopTimer(tcpClose);
}

//
void tcpEstablished(void)
{
    nextTcpState = ESTABLISHED;
}

// Function used to send TCP messages
void sendTcpMessage(uint8_t packet[], uint16_t flags)
{
    uint8_t i;
    uint16_t tcpSize = 0, tmp16;
    uint32_t tmp32 = 0;

    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip       = (ipFrame*)&ether->data;
    tcpFrame *tcp     = (tcpFrame*)((uint8_t*)ip + ((0x45 & 0xF) * 4));

    // Exit function if Sequence number is LT to the most recent sequence number
    // then packet is a retransmission and ignore.
    //if(nextTcpState != CLOSED && tcp->ackNum == tcb.prevSeqNum)
    //    return;

    tcb.prevSeqNum = tcp->seqNum;
    tcb.prevAckNum = tcp->ackNum;

    if(nextTcpState == CLOSED || nextTcpState == CLOSING)
    {

        setAddressInfo(&ether->destAddress, mqttMacAddress, HW_ADD_LENGTH);
        setAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setAddressInfo(&ip->destIp, mqttIpAddress, IP_ADD_LENGTH);
        setAddressInfo(&ip->sourceIp, ipAddress, IP_ADD_LENGTH);
    }
    else
    {
        setAddressInfo(&ether->destAddress, ether->sourceAddress, HW_ADD_LENGTH);
        setAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setAddressInfo(&ip->destIp, ip->sourceIp, IP_ADD_LENGTH);
        setAddressInfo(&ip->sourceIp, ipAddress, IP_ADD_LENGTH);
    }

    ether->frameType = htons(0x0800); // For ipv4

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->revSize        = 0x45;         // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;            // Don't Fragment Flag Set
    ip->ttl            = TIME_TO_LIVE; // Time-to-Live in seconds
    ip->protocol       = 6;            // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    if(nextTcpState == CLOSED)
    {
        mqttSrcPort     += (random32() % 256);
        tcp->destPort   = htons(MQTT_BROKER_PORT);
        tcp->sourcePort = htons(mqttSrcPort);
    }
    else
    {
        tmp16 = tcp->destPort;
        tcp->destPort = tcp->sourcePort;
        tcp->sourcePort = tmp16;
    }

    tcp->checksum      = 0;            // Set checksum to zero before performing calculation
    tcp->urgentPointer = 0;            // Not used in this class
    tcp->window        = htons(1024);

    // If SYN flag = 1, then this is the initial sequence #.
    // The sequence # of actual 1st data byte and the acknowledge #
    // in the corresponding ACK is acknowledge # = sequence # + 1.
    i = 0;
    switch(flags)
    {
    case NOPE: // 0
        tcp->seqNum    = htons32(random32());
        tcp->ackNum    = 0;
        tcp->data[i++] = 0x02; // Kind = 2, Maximum Segment Size
        tcp->data[i++] = 0x04; // Length = 4
        tcp->data[i++] = 0x04; // 1st byte of MSS
        tcp->data[i++] = 0x00; // 2nd byte of MSS
        tcp->data[i++] = 0x01; // NOP
        tcp->data[i++] = 0x01; // NOP
        tcp->data[i++] = 0x04; // SACK Permitted
        tcp->data[i++] = 0x02; // Length
        tcp->dataCtrlFields = htons(0x7002); // Tx ACK
        nextTcpState = SYN_SENT;
        break;
    case FIN: // 1
        tmp32 = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
        tcp->dataCtrlFields = htons(0x5011); // FIN+ACK
        if(nextTcpState == CLOSE_WAIT)
            nextTcpState = LAST_ACK;
        else if(nextTcpState == ESTABLISHED)
            nextTcpState = FIN_WAIT_1;
        break;
    case SYN: // 2
        tmp32       = htons32(tcp->seqNum) + 1;
        tcp->ackNum = htons32(tmp32);
        tmp32       = random32();
        tcp->seqNum = htons32(tmp32);
        tcp->data[i++] = 0x02; // Kind = 2, Maximum Segment Size
        tcp->data[i++] = 0x04; // Length = 4
        tcp->data[i++] = 0x04; // 1st byte of MSS
        tcp->data[i++] = 0x00; // 2nd byte of MSS
        tcp->data[i++] = 0x01; // NOP
        tcp->data[i++] = 0x01; // NOP
        tcp->data[i++] = 0x04; // SACK Permitted
        tcp->data[i++] = 0x02; // Length
        tcp->dataCtrlFields = htons(0x7012); // SYN+ACK
        nextTcpState = SYN_RECEIVED;
        break;
    case RST: // 4
        break;
    case ACK: // 16
        nextTcpState = ESTABLISHED;
        break;
    case FIN_ACK: // 17
        tmp32       = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
        tcp->dataCtrlFields = htons(0x5011); // FIN+ACK
        /*
        switch(nextTcpState)
        {
        case CLOSE_WAIT:
            nextTcpState = LAST_ACK;
            break;
        case CLOSING:
            nextTcpState = TIME_WAIT;
            startOneShotTimer(tcpClose, 2000);
        case FIN_WAIT_1:
            nextTcpState = TIME_WAIT;
            startOneShotTimer(tcpClose, 2000);
            break;
        default:
            break;
        }
        */
        break;
    case SYN_ACK: // SYN+ACK
        tmp32       = htons32(tcp->seqNum) + 1;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
        tcp->dataCtrlFields = htons(0x5010); // Tx ACK
        if(nextTcpState == SYN_SENT)
        {
            nextTcpState = ESTABLISHED;
            startOneShotTimer(mqttMessageEstablished, 10);
        }
        break;
    case PSH_ACK: //
        // Size of TCP Data should be ip->length - size of IP Header - size of TCP Header
        tmp16 = (htons(ip->length) - ((ip->revSize & 0xF) * 4) - ((htons(tcp->dataCtrlFields) & 0xF000) >> 12) * 4);
        tmp32 = htons32(tcp->seqNum) + tmp16;
        tcp->seqNum = tcp->ackNum;
        tcp->ackNum = htons32(tmp32);
        tcp->dataCtrlFields = htons(0x5018); // Tx PSH+ACK
        break;
    default:
        break;
    }

    tcb.currentSeqNum = tcp->seqNum;
    tcb.currentAckNum = tcp->ackNum;

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
    sum += ((ip->protocol & 0xFF) << 8);
    sum += ((tcpSize & 0xFF00));
    sum += ((tcpSize & 0x00FF) << 8);

    // Calculate TCP Header Checksum
    etherSumWords(&tcp->sourcePort, tcpSize);

    tcp->checksum = getEtherChecksum(); // This value is the checksum over both the pseudo-header and the tcp segment

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize);
}

// Lookup requested callback function
_tcpCallback tcpLookup(tcpSysState state, tcpSysEvent event)
{
    uint8_t i, size;

    size = sizeof(tcpStateTransitions)/sizeof(tcpStateTransitions[0]);

    // Revert to LISTEN state if SYN rx'd
    if(event == SYN)
        state = LISTEN;

    for(i = 0; i < size; i++)
    {
        if(state == tcpStateTransitions[i].state && event == tcpStateTransitions[i].event)
        {
            return tcpStateTransitions[i].eventHandler;
        }
    }

    nextTcpState = CLOSED; // If callback function not found in state machine revert to CLOSED state

    return (_tcpCallback)setUpTcb;
}

// Calculate the size of TCP header
uint16_t getTcpHeaderSize(uint8_t size, uint16_t dataOffset)
{
    uint8_t i, numOfWords = 5;

    for(i = 1; i <= size; i++)
    {
        if((i % 4) == 0)
            numOfWords++;
    }

    return (dataOffset &= numOfWords << 12);
}
