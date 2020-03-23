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
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "eeprom.h"
#include "timers.h"
#include "ethernet.h"

#define LEASE_TIME_DIVISOR 650

extern uint32_t transactionId;

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

void sendDhcpDiscoverMessage(uint8_t packet[], uint8_t type[]);
void sendDhcpDeclineMessage(uint8_t packet[]);
void sendDhcpMessage(uint8_t packet[], uint8_t type);
void sendDhcpReleaseMessage(uint8_t packet[]);
void sendDhcpRequestMessage(uint8_t packet[]);
void readDeviceConfig();
bool etherIsDhcp(uint8_t packet[]);
uint8_t dhcpOfferType(uint8_t packet[]);
void getDhcpAckInfo(uint8_t packet[]);
void storeAddressEeprom(uint8_t add1, uint8_t add2, uint8_t add3, uint8_t add4, uint16_t block);
void eraseAddressEeprom();

#endif /* DHCP_H_ */
