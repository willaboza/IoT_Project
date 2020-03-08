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

// uint8_t dhcpServerIpAdd[4];

typedef struct _tcpFrame
{
  uint16_t  srcPort;
  uint16_t  dstPort;
  uint32_t  seqNum;
  uint32_t  ackNum;
  uint16_t  dataOffset; // Contains info for data offset(4-bits), reserved (6-bits), and control bits (6-bits) field,
  uint16_t  window;
  uint16_t  checksum;
  uint16_t  urgentPointer;
  uint8_t   options;
  uint8_t   siaddr[4];
  uint8_t   giaddr[4];
  uint8_t   chaddr[16];
  uint8_t   data[192];
  uint32_t  magicCookie;
  uint8_t   options[];
} tcpFrame;



#endif /* TCP_H_ */
