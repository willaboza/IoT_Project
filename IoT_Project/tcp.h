// tcp.h
// William Bozarth
// Created on: March 8, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef TCP_H_
#define TCP_H_

#include "tcp.h"

#define TIME_TO_LIVE 60

// Transmission control block (Stores info about):
//     - endpoints (IP and port)
//     - status of connection
//     - running data about the packets that are being exchanged
//     - buffers for sending and receiving data
typedef struct _transCtrlBlock
{
    uint32_t prevSeqNum;
    uint32_t prevAckNum;
    uint32_t currentSeqNum;
    uint32_t currentAckNum;
} transCtrlBlock;

extern transCtrlBlock tcb;

typedef enum
{
    NOPE = 0,
    FIN = 1,
    SYN = 2,
    RST = 4,
    ACK = 16,
    FIN_ACK = 17,
    SYN_ACK = 18,
    PSH_ACK = 24
} tcpFlags;

//
// Enumerations of States
//
typedef enum
{
    CLOSED,       //
    LISTEN,       //
    SYN_SENT,     //
    SYN_RECEIVED, //
    ESTABLISHED,  //
    FIN_WAIT_1,   //
    FIN_WAIT_2,   //
    CLOSING,      //
    TIME_WAIT,    //
    CLOSE_WAIT,   //
    LAST_ACK,     //
} tcpSysState;

extern tcpSysState nextTcpState;

//
// Enumerations of Events
//
typedef enum
{
    PASSIVE_OPEN_EVENT = 0, //
    FIN_EVENT = 1,          //
    SYN_EVENT = 2,          //
    APP_CLOSE_EVENT = 7,    //
    ACK_EVENT = 16,         //
    FIN_ACK_EVENT = 17,     //
    SYN_ACK_EVENT = 18,     //
    PSH_ACK_EVENT = 24      //
} tcpSysEvent;

typedef tcpSysState(*_tcpCallback)(uint8_t packet[], uint16_t flags);

//
// Structures
//
typedef struct
{
    tcpSysState state;
    tcpSysEvent event;
    _tcpCallback eventHandler;
} tcpStateMachine;

typedef struct _tcpFrame // 20 Bytes in Length
{
  uint16_t  sourcePort;     // 2
  uint16_t  destPort;       // 2
  uint32_t  seqNum;         // 4
  uint32_t  ackNum;         // 4
  uint16_t  dataCtrlFields; // 2
  uint16_t  window;         // 2 = 16 bytes
  uint16_t  checksum;
  uint16_t  urgentPointer;
  uint8_t   data[0];
} tcpFrame;

void dupTcpMsg(void);
bool etherIsTcp(uint8_t packet[]);
uint16_t etherIsTcpMsgType(uint8_t packet[]);
void tcpAckReceived(uint8_t packet[]);
void sendTcpMessage(uint8_t packet[], uint16_t flags);
bool checkForDuplicates(uint8_t packet[]);
_tcpCallback tcpLookup(tcpSysState state, tcpSysEvent event);
void setUpTcb(void);
void tcpEstablished(void);
void tcpClose(void);
uint16_t getTcpHeaderSize(uint8_t size, uint16_t dataOffset);

#endif /* TCP_H_ */
