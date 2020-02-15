/*
 * dhcp.h
 *
 *  Created on: Feb 12, 2020
 *      Author: willi
 */

#ifndef DHCP_H_
#define DHCP_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "eeprom.h"

// uint8_t dhcpServerIpAdd[4];

typedef struct _dhcpFrame
{
  uint8_t   op;
  uint8_t   htype;
  uint8_t   hlen;
  uint8_t   hops;
  uint32_t  xid;
  uint16_t  secs;
  uint16_t  flags;
  uint8_t   ciaddr[4];
  uint8_t   yiaddr[4];
  uint8_t   siaddr[4];
  uint8_t   giaddr[4];
  uint8_t   chaddr[16];
  uint8_t   data[192];
  uint32_t  magicCookie;
  uint8_t   options[0];
} dhcpFrame;

void sendDhcpMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[]);

#endif /* DHCP_H_ */
