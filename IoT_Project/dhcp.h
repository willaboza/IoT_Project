// dhcp.h
// William Bozarth
// Created on: February 12, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef DHCP_H_
#define DHCP_H_

#include "dhcp.h"

#define LEASE_TIME_DIVISOR 1

extern uint32_t transactionId;
extern bool dhcpIpLeased;

//
// Enumerations of States
//
typedef enum
{
    NONE,        // DHCP mode disabled
    INIT,        // Process of acquiring a lease begins
    SELECTING,   // Client is waiting for an OFFER from one or more servers
    REQUESTING,  // Client waits to hear from server request was sent to
    INIT_REBOOT, // When client has valid lease at start-up or reboot
    REBOOTING,   // Client with assigned address waits for confirmation reply from server
    BOUND,       // Valid lease obtained and in NORMAL operating state
    RENEWING,    // Client tries to renew lease
    REBINDING,   // Lease renewal FAILED and client seeks lease extension from other servers
} dhcpSysState;

extern dhcpSysState nextDhcpState;

//
// Enumerations of Events
//
typedef enum
{
    NO_EVENT,            // No event listed below happened
    DHCPDISCOVERY_EVENT, // Send DHCPDISCOVER
    DHCPOFFER_EVENT,
    DHCPREQUEST_EVENT,   // Send DHCPREQUEST
    DHCPDECLINE_EVENT,   // IP address taken so send DHCPDECLINE
    DHCPACK_EVENT,       // Start Lease with IP Address and Set Timers
    DHCPNACK_EVENT,      // Go back to INIT state if DHCPNAK rx'd
    DHCPRELEASE_EVENT,   // Terminate Lease
    DHCPINFORM_EVENT
} dhcpSysEvent;

typedef dhcpSysState(*_dhcpCallback)(uint8_t packet[]);

//
// Structures
//
typedef struct
{
    dhcpSysState state;
    dhcpSysEvent event;
    _dhcpCallback eventHandler;
} dhcpStateMachine;

typedef struct _dhcpFrame
{
  uint8_t   op;
  uint8_t   htype;
  uint8_t   hlen;
  uint8_t   hops;
  uint32_t  xid;
  uint16_t  secs;
  uint16_t  flags;     // 12
  uint8_t   ciaddr[4]; // 16
  uint8_t   yiaddr[4]; // 20
  uint8_t   siaddr[4]; // 24
  uint8_t   giaddr[4]; // 28
  uint8_t   chaddr[16]; // 44
  uint8_t   data[192];  // 236
  uint32_t  magicCookie; // 240
  uint8_t   options[0];
} dhcpFrame;

void exitDhcpMode(void);
void sendDhcpDeclineMessage(uint8_t packet[]);
void sendDhcpReleaseMessage(uint8_t packet[]);
void sendDhcpRequestMessage(uint8_t packet[]);
bool readDeviceConfig(void);
bool etherIsDhcp(uint8_t packet[]);
uint8_t dhcpOfferType(uint8_t packet[]);
void sendDhcpInformMessage(uint8_t packet[]);
void sendDhcpDiscoverMessage(uint8_t packet[]);
void setDhcpOption(void* data, uint8_t option, uint8_t optionLength, uint8_t dhcpLength);
void LeaseAddressHandler(uint8_t packet[]);
void dhcpNackHandler(uint8_t packet[]);
void renewalTimer(void);
void rebindTimer(void);
void arpResponseTimer(void);
void leaseExpHandler(void);
void setDhcpAddressInfo(void* data, uint8_t add[], uint8_t sizeInBytes);
void waitTimer(void);
void resetTimers(void);

_dhcpCallback dhcpLookup(dhcpSysState state, dhcpSysEvent event);

#endif /* DHCP_H_ */
