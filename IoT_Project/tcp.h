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

extern bool openState;
extern bool establishedState;
extern bool closeState;

typedef struct _tcpFrame // 20 Bytes in Length
{
  uint16_t  sourcePort;
  uint16_t  destPort;
  uint32_t  seqNum;
  uint32_t  ackNum;
  uint16_t  dataCtrlFields;
  uint16_t  window;
  uint16_t  checksum;
  uint16_t  urgentPointer;
  uint8_t   data[0];
} tcpFrame;

bool etherIsTcp(uint8_t packet[]);
uint8_t etherIsTcpMsgType(uint8_t packet[]);
void sendTcpMessage(uint8_t packet[], uint16_t flags);

#endif /* TCP_H_ */
