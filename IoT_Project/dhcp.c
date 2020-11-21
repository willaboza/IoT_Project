// dhcp.c
// William Bozarth
// Created on: February 12, 2020

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
#include "dhcp.h"
#include "eeprom.h"
#include "uart0.h"
#include "timers.h"
#include "ethernet.h"
#include "mqtt.h"

uint32_t transactionId = 0;
bool dhcpIpLeased = false;

dhcpSysState nextDhcpState = INIT;

dhcpStateMachine stateTransitions [] =
{
    {NONE,        NO_EVENT,            (_dhcpCallback)sendDhcpReleaseMessage},  // Handles when DHCP mode disabled
    {INIT,        DHCPDISCOVERY_EVENT, (_dhcpCallback)sendDhcpDiscoverMessage}, // DiscoverHandler
    {INIT,        DHCPINFORM_EVENT,    (_dhcpCallback)sendDhcpInformMessage},   //
    {INIT,        DHCPACK_EVENT,       (_dhcpCallback)exitDhcpMode},            //
    {SELECTING,   DHCPOFFER_EVENT,     (_dhcpCallback)sendDhcpRequestMessage},  //
    {REQUESTING,  DHCPACK_EVENT,       (_dhcpCallback)LeaseAddressHandler},     //
    {REQUESTING,  DHCPNACK_EVENT,      (_dhcpCallback)dhcpNackHandler},         //
    {INIT_REBOOT, DHCPREQUEST_EVENT,   (_dhcpCallback)sendDhcpRequestMessage},  // InitRebootHandler
    {REBOOTING,   DHCPACK_EVENT,       (_dhcpCallback)LeaseAddressHandler},     //
    {REBOOTING,   DHCPNACK_EVENT,      (_dhcpCallback)dhcpNackHandler},         //
    {BOUND,       DHCPREQUEST_EVENT,   (_dhcpCallback)sendDhcpRequestMessage},  //
    {BOUND,       DHCPRELEASE_EVENT,   (_dhcpCallback)sendDhcpReleaseMessage},  //
    {RENEWING,    DHCPREQUEST_EVENT,   (_dhcpCallback)sendDhcpRequestMessage},  //
    {RENEWING,    DHCPACK_EVENT,       (_dhcpCallback)resetTimers},             //
    {RENEWING,    DHCPNACK_EVENT,      (_dhcpCallback)dhcpNackHandler},         //
    {REBINDING,   DHCPACK_EVENT,       (_dhcpCallback)resetTimers},             //
    {REBINDING,   NO_EVENT,            (_dhcpCallback)leaseExpHandler},         //
    {REBINDING,   DHCPNACK_EVENT,      (_dhcpCallback)dhcpNackHandler}          //
};

// Function to determine if DHCP mode ENABLED or DISABLED
bool readDeviceConfig(void)
{
    if(readEeprom(0x0010) == 0xFFFFFFFF) // If statement evaluates to TRUE if NOT in DHCP Mode
    {
        // Initialize ENCJ2860 module
        initEthernetInterface(false);

        // Output to terminal configuration info
        displayConnectionInfo();

        return false;
    }
    else // DHCP Mode is enabled
    {
        nextDhcpState = INIT_REBOOT;

        // Check if address info is stored in EEPROM
        getAddressInfo(ipAddress, 0x0011, 4);        // Get IP address
        getAddressInfo(ipGwAddress, 0x0012, 4);      // Get GW address
        getAddressInfo(ipDnsAddress, 0x0013, 4);     // Get DNS address
        getAddressInfo(serverIpAddress, 0x0013, 4);  // Get DHCP SERVER address
        getAddressInfo(ipSubnetMask, 0x0014, 4);     // Get SN mask
        getAddressInfo(serverMacAddress, 0x0015, 6); // Get Server MAC Address

        // Initialize ENCJ2860 module
        initEthernetInterface(true);

        // Output to terminal configuration info
        displayConnectionInfo();

        return true;
    }
}

// Handles exiting dhcp mode
void exitDhcpMode(void){nextDhcpState = INIT;}

// Transmit DHCPDISCOVER message
void sendDhcpDiscoverMessage(uint8_t packet[])
{
    uint8_t i, n;
    uint32_t dhcpSize = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    // Ether Type for ipv4  is 0x0800
    ether->frameType = htons(0x0800);

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = 0xFF;
        ip->sourceIp[i] = 0;
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 64; // Time-to-Live in seconds
    ip->protocol       = 17; // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list).

    // Fill UDP Frame
    udp->destPort   = 0x4300; // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = 0x4400; // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;      // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    dhcp->xid   = transactionId = htons32(random32());
    dhcp->secs  = 0;
    dhcp->flags = 0;

    // Start of adding Options for REQUEST message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = DHCPDISCOVERY_EVENT;

    // Option 61, Client-Identifier
    dhcp->options[n++] = 61;
    dhcp->options[n++] = 7;  // Length
    dhcp->options[n++] = 1;  // Hardware Type
    dhcp->options[n++] = macAddress[0];
    dhcp->options[n++] = macAddress[1];
    dhcp->options[n++] = macAddress[2];
    dhcp->options[n++] = macAddress[3];
    dhcp->options[n++] = macAddress[4];
    dhcp->options[n++] = macAddress[5];

    // Option 55, Parameter Request List
    dhcp->options[n++] = 55;
    dhcp->options[n++] = 4;  // Length
    dhcp->options[n++] = 1;  // Subnet Mask (for client)
    dhcp->options[n++] = 3;  // Router Option
    dhcp->options[n++] = 15; // Domain Name
    dhcp->options[n++] = 6;  // Domain Name Server Option

    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = 0;
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
        dhcp->siaddr[i] = 0;
    }

    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < 16; i++)
    {
        if(i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
    }

    // Fill DHCP data field with ZEROs
    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = 0x63538263; // big-endian of 0x63825363

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + n);

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    sum = 0;
    udp->length = htons(8 + dhcpSize);
    etherSumWords(ip->sourceIp, 8);
    sum += (ip->protocol & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);

    nextDhcpState = SELECTING; // Set to next state
}


// Send DHCPINFORM Message
void sendDhcpInformMessage(uint8_t packet[])
{
    uint8_t i, n;
    uint32_t dhcpSize = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;


    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    dhcp->flags = htons(0x8000); // 0x8000 to set Broadcast Flag

    ether->frameType = 0x0008; // For ipv4 (0x0800) big-endian value stored in Frame Type

    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = 0xFF;
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 64; // Time-to-Live in seconds
    ip->protocol       = 17;  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    // Fill UDP Frame
    udp->destPort   = 0x4300; // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = 0x4400; // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;      // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    dhcp->secs  = 0;

    dhcp->xid = transactionId = htons32(random32());

    // Start of adding Options for REQUEST message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = 8;
    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;


    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = ipAddress[i];
    }

    // Set YIADDR and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < 16; i++)
    {
        if(i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
    }

    // Fill DHCP data field with ZEROs
    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = 0x63538263; // big-endian of 0x63825363

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + n);

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    sum = 0;
    udp->length = htons(8 + dhcpSize);
    etherSumWords(ip->sourceIp, 8);
    sum += (ip->protocol & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);

    // Set DHCP to next state
    nextDhcpState = INIT;
}

// Function to handle DHCPREQUEST Messages
void sendDhcpRequestMessage(uint8_t packet[])
{
    uint8_t i, n;
    uint32_t dhcpSize = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    // Fill ether and IP frames
    switch(nextDhcpState)
    {
    case SELECTING:
        setDhcpAddressInfo(&ether->destAddress, broadcastAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ip->destIp, broadcastAddress, IP_ADD_LENGTH);
        setDhcpAddressInfo(&ip->sourceIp, unicastAddress, IP_ADD_LENGTH);

        // Set next state
        nextDhcpState = REQUESTING;
        break;
    case INIT_REBOOT:
        setDhcpAddressInfo(&ether->destAddress, broadcastAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ip->destIp, serverIpAddress, IP_ADD_LENGTH);
        setDhcpAddressInfo(&ip->sourceIp, ipAddress, IP_ADD_LENGTH);

        // Set next state
        nextDhcpState = REBOOTING;
        break;
    case BOUND:
        //setDhcpAddressInfo(&ether->destAddress, broadcastAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ether->destAddress, serverMacAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ip->destIp, serverIpAddress, IP_ADD_LENGTH);
        setDhcpAddressInfo(&ip->sourceIp, ipAddress, IP_ADD_LENGTH);

        // Set next state
        nextDhcpState = RENEWING;
        break;
    case RENEWING:
        setDhcpAddressInfo(&ether->destAddress, broadcastAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ether->sourceAddress, macAddress, HW_ADD_LENGTH);
        setDhcpAddressInfo(&ip->destIp, broadcastAddress, IP_ADD_LENGTH);
        setDhcpAddressInfo(&ip->sourceIp, ipAddress, IP_ADD_LENGTH);

        // Set next state
        nextDhcpState = REBINDING;
        break;
    default:
        break;
    }

    ether->frameType = htons(0x0800);

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 64; // Time-to-Live in seconds
    ip->protocol       = 17; // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list).

    // Fill UDP Frame
    udp->destPort   = 0x4300; // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = 0x4400; // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;      // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    if(nextDhcpState == SELECTING)
        dhcp->xid   = transactionId;
    else
        dhcp->xid = transactionId = htons32(random32());
    dhcp->secs  = 0;
    if(nextDhcpState == RENEWING)
        dhcp->flags = htons(0x0000); // Unicast
    else
        dhcp->flags = htons(0x8000); // Broadcast

    // Start of adding Options for REQUEST message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = DHCPREQUEST_EVENT;

    // Option 61, Client-Identifier
    dhcp->options[n++] = 61;
    dhcp->options[n++] = 7;  // Length
    dhcp->options[n++] = 1;  // Hardware Type
    dhcp->options[n++] = macAddress[0];
    dhcp->options[n++] = macAddress[1];
    dhcp->options[n++] = macAddress[2];
    dhcp->options[n++] = macAddress[3];
    dhcp->options[n++] = macAddress[4];
    dhcp->options[n++] = macAddress[5];

    // Option 50, Requested IP Address
    dhcp->options[n++] = 50;
    dhcp->options[n++] = 4;  // Length
    dhcp->options[n++] = ipAddress[0];
    dhcp->options[n++] = ipAddress[1];
    dhcp->options[n++] = ipAddress[2];
    dhcp->options[n++] = ipAddress[3];

    // Option 54, Server Identifier
    dhcp->options[n++] = 54;
    dhcp->options[n++] = 4;  // Length
    dhcp->options[n++] = serverIpAddress[0];
    dhcp->options[n++] = serverIpAddress[1];
    dhcp->options[n++] = serverIpAddress[2];
    dhcp->options[n++] = serverIpAddress[3];

    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < IP_ADD_LENGTH; i++)
    {
        if(nextDhcpState == SELECTING)
            dhcp->ciaddr[i] = dhcp->yiaddr[i];
        else
            dhcp->ciaddr[i] = ipAddress[i];
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
        // dhcp->siaddr[i] = 0;
    }

    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < 16; i++)
    {
        if(i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
    }

    // Fill DHCP data field with ZEROs
    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    // big-endian of 0x63825363
    dhcp->magicCookie = 0x63538263;

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + n);

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    sum = 0;
    udp->length = htons(8 + dhcpSize);
    etherSumWords(ip->sourceIp, 8);
    sum += (ip->protocol & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);
}

// DHCPRELEASE Message
void sendDhcpReleaseMessage(uint8_t packet[])
{
    uint8_t i, n;
    uint16_t temp16;
    uint32_t dhcpSize = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = 0x0008; // For ipv4 (0x0800) big-endian value stored in Frame Type

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = serverIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 10; // Time-to-Live in seconds
    ip->protocol       = 17; // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    // Fill UDP Frame
    udp->destPort   = 0x4300; // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = 0x4400; // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;      // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    dhcp->xid = transactionId = htons32(random32());
    dhcp->secs  = 0;

    dhcp->flags = htons(0x0000);

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = ipAddress[i];
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < 16; i++)
    {
        if(i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
    }

    // Fill 192 byte data field with zeros
    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = 0x63538263; // big-endian of 0x63825363

    // Start of adding Options for DHCP message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = 7;

    // Option 54
    dhcp->options[n++] = 54;
    dhcp->options[n++] = 4;
    dhcp->options[n++] = dhcp->siaddr[0];
    dhcp->options[n++] = dhcp->siaddr[1];
    dhcp->options[n++] = dhcp->siaddr[2];
    dhcp->options[n++] = dhcp->siaddr[3];

    // End Of Options
    dhcp->options[n++] = 255;

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + n);

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    sum = 0;
    udp->length = htons(8 + dhcpSize);
    etherSumWords(ip->sourceIp, 8);
    temp16 = ip->protocol;
    sum += (temp16 & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);

    nextDhcpState = INIT;
}

// DHCPDECLINE Message
void sendDhcpDeclineMessage(uint8_t packet[])
{
    uint8_t i, n;
    uint32_t dhcpSize = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit Version field = 4 and IHL = 5 indicating the size is 20 bytes. (69 decimal or 0x45h)
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    ether->frameType = 0x0008; // For ipv4 (0x0800) big-endian value stored in Frame Type

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = serverIpAddress[i];
        ip->sourceIp[i] = ipAddress[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 10; // Time-to-Live in seconds
    ip->protocol       = 17; // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    // Fill UDP Frame
    udp->destPort   = 0x4300; // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = 0x4400; // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;      // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    dhcp->xid   = transactionId;
    dhcp->secs  = 0;

    dhcp->flags = htons(0x0000);

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = 0;
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < 16; i++)
    {
        if(i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
    }

    // Fill 192 byte data field with zeros
    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = 0x63538263; // big-endian of 0x63825363

    // Start of adding Options for DHCP message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = 4;

    // End Of Options
    dhcp->options[n++] = 255;

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + n);

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherCalcIpChecksum(ip);

    // 32-bit sum over pseudo-header
    sum = 0;
    udp->length = htons(8 + dhcpSize);
    etherSumWords(ip->sourceIp, 8);
    sum += (ip->protocol & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);

    nextDhcpState = INIT;
}

// Function to check if received packet is DHCP Message
bool etherIsDhcp(uint8_t packet[])
{
    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    if(htons(udp->destPort) == 68 && htons(udp->sourcePort) == 67)
        return true;

    return false;
}

// Extract DHCP Option info from incoming packets
uint8_t dhcpOfferType(uint8_t packet[])
{
    uint8_t i = 0, dhcpLength, option, type;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    // Get size of DHCP options
    // size = udp length - udp header size (8 bytes) - dhcp header size (240 bytes)
    dhcpLength = udp->length - 8 - 240;

    // Store transaction ID of incoming packet
    transactionId = dhcp->xid;

    etherSetServerMacAddress(ether->sourceAddress[0], ether->sourceAddress[1], ether->sourceAddress[2], ether->sourceAddress[3], ether->sourceAddress[4], ether->sourceAddress[5]);

    // Process DHCP options
    // (255 = END of Options)
    while(dhcp->options[i] != 0xFF || i < dhcpLength)
    {
        option = dhcp->options[i];

        switch(option)
        {
        case 1: // Option 1 (Subnet Mask)
            i += 2;
            ipSubnetMask[0] = dhcp->options[i++];
            ipSubnetMask[1] = dhcp->options[i++];
            ipSubnetMask[2] = dhcp->options[i++];
            ipSubnetMask[3] = dhcp->options[i];
            break;
        case 51: // Lease Time
            i += 2;
            leaseTime |= (dhcp->options[i++] << 24);
            leaseTime |= (dhcp->options[i++] << 16);
            leaseTime |= (dhcp->options[i++] << 8);
            leaseTime |= dhcp->options[i];
            break;
        case 53: // DHCP Message Type
            i += 2;
            type = dhcp->options[i];

            // Set IP provided by server if message type = DHCPOFFER
            if(type == DHCPOFFER_EVENT)
                etherSetIpAddress(dhcp->yiaddr[0], dhcp->yiaddr[1], dhcp->yiaddr[2], dhcp->yiaddr[3]);
            break;
        case 54: // DHCP Server Identifier
            i += 2;
            serverIpAddress[0] = dhcp->options[i++];
            serverIpAddress[1] = dhcp->options[i++];
            serverIpAddress[2] = dhcp->options[i++];
            serverIpAddress[3] = dhcp->options[i];
            break;
        default:
            // Increment by length of DHCP option
            i += dhcp->options[++i];
            break;
        }

        i++;
    }
    return type;
}

// Add DHCP Option info
void setDhcpOption(void* data, uint8_t option, uint8_t optionLength, uint8_t dhcpLength)
{
    //uint8_t* pData = (uint8_t*)data;
}

// Return to INIT state if DHCPNAK Rx'd
void dhcpNackHandler(uint8_t packet[])
{
    // Stop Timers before changing states
    stopTimer(leaseExpHandler);
    stopTimer(renewalTimer);
    stopTimer(rebindTimer);
    stopTimer(arpResponseTimer);

    // Send another DHCPDISCOVER message
    (*dhcpLookup(INIT, DHCPDISCOVERY_EVENT))(packet);

    nextDhcpState = INIT;
}

// DHCP "WAIT" TIMER, sends DHCPDISOVER message after 10 seconds has elapsed
void waitTimer(void)
{
    uint8_t data[MAX_PACKET_SIZE] = {0};

    // Send another DHCPDISCOVER message
    (*dhcpLookup(INIT, DHCPDISCOVERY_EVENT))(data);
}

// Start lease offer for IP address
void LeaseAddressHandler(uint8_t packet[])
{
    // Start lease Timer
    startOneShotTimer(leaseExpHandler, leaseTime);

    // Start Renew Timer
    startOneShotTimer(renewalTimer, (leaseTime / (2 * LEASE_TIME_DIVISOR)));

    // Start Rebind Timer
    startOneShotTimer(rebindTimer, ((leaseTime * 7) / (8 * LEASE_TIME_DIVISOR)));

    // Send Gratuitous ARP Request
    sendArpProbe(packet);

    // Start ARP Response timer
    startOneShotTimer(arpResponseTimer, 2);
}

// Re-start renewal and rebind timers
void resetTimers(void)
{
    // Start Lease Timer
    restartTimer(leaseExpHandler);

    // Start Renew Timer
    restartTimer(renewalTimer);

    // Start Rebind Timer
    restartTimer(rebindTimer);

    nextDhcpState = BOUND;
}

// Unicast DHCPREQUEST message when renewal timer elapses
void renewalTimer(void)
{
    uint8_t data[MAX_PACKET_SIZE] = {0};

    (*dhcpLookup(BOUND, DHCPREQUEST_EVENT))(data);

    setPinValue(BLUE_LED, 1);

    startOneShotTimer(clearBlueLed, 2);
}

// Broadcast DHCPREQUEST message when renewal timer elapses
void rebindTimer(void)
{
    uint8_t data[MAX_PACKET_SIZE] = {0};

    (*dhcpLookup(RENEWING, DHCPREQUEST_EVENT))(data);

    setPinValue(GREEN_LED, 1);

    startOneShotTimer(clearGreenLed, 2);
}

//
void leaseExpHandler(void)
{
    uint8_t data[MAX_PACKET_SIZE] = {0};

    // Stop Timers before changing states
    stopTimer(leaseExpHandler);
    stopTimer(renewalTimer);
    stopTimer(rebindTimer);
    stopTimer(arpResponseTimer);

    // Send another DHCPDISCOVER message
    (*dhcpLookup(INIT, DHCPDISCOVERY_EVENT))(data);

    nextDhcpState = INIT;
}

// 2-Second Timer to wait for any A
void arpResponseTimer(void)
{
    uint8_t data[MAX_PACKET_SIZE] = {0};

    sendArpAnnouncement(data);

    stopTimer(arpResponseTimer);

    // Store device configuration information
    storeAddressEeprom(ipAddress, 0x0011, 4);        // Store device IP address
    storeAddressEeprom(ipGwAddress, 0x0012, 4);      // Store GW address
    storeAddressEeprom(serverIpAddress, 0x0013, 4);  // Store server IP address
    storeAddressEeprom(ipSubnetMask, 0x0014, 4);     // Store SN mask
    storeAddressEeprom(serverMacAddress, 0x0015, 6); // Store Server MAC address

    sendGratuitousArpResponse(data);

    // Transition to next state
    nextDhcpState = BOUND;
}

// Use to set the various ether or IP address info
void setDhcpAddressInfo(void* data, uint8_t add[], uint8_t sizeInBytes)
{
    uint8_t i;
    uint8_t* pData = (uint8_t*)data;

    for (i = 0; i < sizeInBytes; i++)
    {
        *pData = add[i];
        pData++;
    }
}

// Lookup requested callback function
_dhcpCallback dhcpLookup(dhcpSysState state, dhcpSysEvent event)
{
    uint8_t i, size;

    size = sizeof(stateTransitions)/sizeof(stateTransitions[0]);

    for(i = 0; i < size; i++)
    {
        if(state == stateTransitions[i].state && event == stateTransitions[i].event)
        {
            return stateTransitions[i].eventHandler;
        }
    }

    // Modify this return later to return NACK_HANDLER function
    return NULL;
}
