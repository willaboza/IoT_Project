/*
 * ethernet.h
 *
 *  Created on: Feb 12, 2020 by William Bozarth
 *      Author: Jason Losh
 *
 */

#ifndef ETHERNET_H_
#define ETHERNET_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "wait.h"
#include "gpio.h"
#include "uart0.h"
#include "spi.h"
#include "mqtt.h"

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

// User IP and MAC Unique ID for Static Mode
#define UNIQUE_ID 106

// Pins
#define CS  PORTA,3
#define WOL PORTB,3
#define INT PORTC,6

// Ether registers
#define ERDPTL      0x00
#define ERDPTH      0x01
#define EWRPTL      0x02
#define EWRPTH      0x03
#define ETXSTL      0x04
#define ETXSTH      0x05
#define ETXNDL      0x06
#define ETXNDH      0x07
#define ERXSTL      0x08
#define ERXSTH      0x09
#define ERXNDL      0x0A
#define ERXNDH      0x0B
#define ERXRDPTL    0x0C
#define ERXRDPTH    0x0D
#define ERXWRPTL    0x0E
#define ERXWRPTH    0x0F
#define EIE         0x1B
#define EIR         0x1C
#define RXERIF      0x01
#define TXERIF      0x02
#define TXIF        0x08
#define PKTIF       0x40
#define ESTAT       0x1D
#define CLKRDY      0x01
#define TXABORT     0x02
#define ECON2       0x1E
#define PKTDEC      0x40
#define ECON1       0x1F
#define RXEN        0x04
#define TXRTS       0x08
#define ERXFCON     0x38
#define EPKTCNT     0x39
#define MACON1      0x40
#define MARXEN      0x01
#define RXPAUS      0x04
#define TXPAUS      0x08
#define MACON2      0x41
#define MARST       0x80
#define MACON3      0x42
#define FULDPX      0x01
#define FRMLNEN     0x02
#define TXCRCEN     0x10
#define PAD60       0x20
#define MACON4      0x43
#define MABBIPG     0x44
#define MAIPGL      0x46
#define MAIPGH      0x47
#define MACLCON1    0x48
#define MACLCON2    0x49
#define MAMXFLL     0x4A
#define MAMXFLH     0x4B
#define MICMD       0x52
#define MIIRD       0x01
#define MIREGADR    0x54
#define MIWRL       0x56
#define MIWRH       0x57
#define MIRDL       0x58
#define MIRDH       0x59
#define MAADR1      0x60
#define MAADR0      0x61
#define MAADR3      0x62
#define MAADR2      0x63
#define MAADR5      0x64
#define MAADR4      0x65
#define MISTAT      0x6A
#define MIBUSY      0x01
#define ECOCON      0x75

// Ether phy registers
#define PHCON1      0x00
#define PDPXMD      0x0100
#define PHSTAT1     0x01
#define LSTAT       0x0400
#define PHCON2      0x10
#define HDLDIS      0x0100
#define PHLCON      0x14

// Packets
#define IP_ADD_LENGTH 4
#define HW_ADD_LENGTH 6

#define ETHER_UNICAST        0x80
#define ETHER_BROADCAST      0x01
#define ETHER_MULTICAST      0x02
#define ETHER_HASHTABLE      0x04
#define ETHER_MAGICPACKET    0x08
#define ETHER_PATTERNMATCH   0x10
#define ETHER_CHECKCRC       0x20

#define ETHER_HALFDUPLEX     0x00
#define ETHER_FULLDUPLEX     0x100

#define LOBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
extern uint8_t  nextPacketLsb;
extern uint8_t  nextPacketMsb;
extern uint8_t  sequenceId;
extern uint32_t sum;
extern uint8_t  macAddress[HW_ADD_LENGTH];
extern uint8_t  serverMacAddress[HW_ADD_LENGTH];
extern uint8_t  broadcastAddress[HW_ADD_LENGTH];
extern uint8_t  unicastAddress[HW_ADD_LENGTH];
extern uint8_t  serverIpAddress[IP_ADD_LENGTH];
extern uint8_t  ipAddress[IP_ADD_LENGTH];
extern uint8_t  ipSubnetMask[IP_ADD_LENGTH];
extern uint8_t  ipGwAddress[IP_ADD_LENGTH];
extern uint8_t  ipDnsAddress[IP_ADD_LENGTH];
extern bool     dhcpEnabled;

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

// This M4F is little endian (TI hardwired it this way)
// Network byte order is big endian
// Must interpret uint16_t in reverse order

typedef struct _enc28j60Frame // 4-bytes
{
  uint16_t size;
  uint16_t status;
  uint8_t  data;
} enc28j60Frame;

typedef struct _etherFrame // 14-bytes
{
  uint8_t  destAddress[6];
  uint8_t  sourceAddress[6];
  uint16_t frameType;
  uint8_t  data;
} etherFrame;

// See RFC791 Section 3.1 for Internet Header Format
// https://tools.ietf.org/html/rfc791#section-3.1
typedef struct _ipFrame // minimum 20 bytes
{
  uint8_t  revSize;
  uint8_t  typeOfService;
  uint16_t length;
  uint16_t id;
  uint16_t flagsAndOffset;
  uint8_t  ttl;
  uint8_t  protocol;
  uint16_t headerChecksum;
  uint8_t  sourceIp[4];
  uint8_t  destIp[4];
} ipFrame;

typedef struct _icmpFrame
{
  uint8_t  type;
  uint8_t  code;
  uint16_t check;
  uint16_t id;
  uint16_t seq_no;
  uint8_t  data;
} icmpFrame;

typedef struct _arpFrame // 28
{
  uint16_t hardwareType;
  uint16_t protocolType;
  uint8_t  hardwareSize;
  uint8_t  protocolSize;
  uint16_t op;
  uint8_t  sourceAddress[6];
  uint8_t  sourceIp[4];
  uint8_t  destAddress[6];
  uint8_t  destIp[4];
} arpFrame;

typedef struct _udpFrame // 8 bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint16_t length;
  uint16_t check;
  uint8_t  data;
} udpFrame;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void etherInit(uint16_t mode);
bool etherIsLinkUp(void);

bool etherIsDataAvailable(void);
bool etherIsOverflow(void);
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize);
bool etherPutPacket(uint8_t packet[], uint16_t size);

bool etherIsIp(uint8_t packet[]);
bool etherIsIpUnicast(uint8_t packet[]);

bool etherIsPingRequest(uint8_t packet[]);
void etherSendPingResponse(uint8_t packet[]);

bool etherIsArpRequest(uint8_t packet[]);
bool etherIsArpResponse(uint8_t packet[]);
void etherSendArpResponse(uint8_t packet[]);
void etherSendArpRequest(uint8_t packet[]);

bool etherIsUdp(uint8_t packet[]);
uint8_t* etherGetUdpData(uint8_t packet[]);
void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize);

void etherEnableDhcpMode(void);
void etherDisableDhcpMode(void);
bool etherIsDhcpEnabled(void);
bool etherIsIpValid(void);
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpAddress(uint8_t ip[4]);
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpGatewayAddress(uint8_t ip[4]);
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3);
void etherGetIpSubnetMask(uint8_t mask[4]);
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);
void etherSetServerMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);
void etherGetMacAddress(uint8_t mac[6]);
void etherSumWords(void* data, uint16_t sizeInBytes);
void etherCalcIpChecksum(ipFrame* ip);
uint16_t getEtherChecksum();
void setDnsAddress(uint8_t dns0, uint8_t dns1, uint8_t dns2, uint8_t dns3);
void getDnsAddress(uint8_t dns[4]);
void initEthernetInterface(bool ok);

uint16_t htons(uint16_t value);
#define ntohs htons
uint32_t htons32(uint32_t value);

void displayConnectionInfo(void);
void displayIfconfigInfo(void);
void setStaticNetworkAddresses(void);

#endif /* ETHERNET_H_ */
