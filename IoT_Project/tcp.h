/*
 * tcp.h
 *
 *  Created on: Mar 8, 2020
 *      Author: willi
 *
 *      See RFC793 section 3.1 for TCP Header Format
 */

#ifndef TCP_H_
#define TCP_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "ethernet.h"
#include "timers.h"
#include "mqtt.h"

extern bool listenState;
extern bool establishedState;
extern bool closeState;
extern uint32_t prevSeqNum;
extern uint32_t prevAckNum;

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

bool etherIsTcp(uint8_t packet[]);
uint8_t etherIsTcpMsgType(uint8_t packet[]);
void sendTcpMessage(uint8_t packet[], uint16_t flags);
void getTcpData(uint8_t packet[]);
bool checkForDuplicates(uint8_t packet[]);

#endif /* TCP_H_ */
