/*
 * dhcp.c
 *
 *  Created on: Feb 12, 2020
 *      Author: William Bozarth
 *
 *  Functions for Client side of DHCP are below.
 */

#include "dhcp.h"

uint32_t transactionId = 0;

// Function to determine if DHCP mode ENABLED or DISABLED
void readDeviceConfig()
{
    if(readEeprom(0x0010) != 0xFFFFFFFF)
    {
        dhcpRequestType = 0;  // Broadcast DHCPREQUEST as no
        rebindRequest = true;
        releaseRequest = renewRequest = false;

        initEthernetInterface();
        etherEnableDhcpMode();

        putsUart0("Starting eth0\r\n");
        displayConnectionInfo();
        putsUart0("\r\n");
    }
    else
    {
        uint32_t num = 0;

        etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
        etherDisableDhcpMode();

        if((num = readEeprom(0x0011)) != 0xFFFFFFFF) // IP
        {
            ipAddress[0] = (num >> 24);
            ipAddress[1] = (num >> 16);
            ipAddress[2] = (num >> 8);
            ipAddress[3] = num;
        }
        num = 0;
        if((num = readEeprom(0x0012)) != 0xFFFFFFFF) // GW
        {
            ipGwAddress[0] = (num >> 24);
            ipGwAddress[1] = (num >> 16);
            ipGwAddress[2] = (num >> 8);
            ipGwAddress[3] = num;
        }
        num = 0;
        if((num = readEeprom(0x0013)) != 0xFFFFFFFF) // DNS
        {
            serverIpAddress[0] = ipDnsAddress[0] = (num >> 24);
            serverIpAddress[0] = ipDnsAddress[1] = (num >> 16);
            serverIpAddress[0] = ipDnsAddress[2] = (num >> 8);
            serverIpAddress[0] = ipDnsAddress[3] = num;
        }
        num = 0;
        if((num = readEeprom(0x0014)) != 0xFFFFFFFF) //SN
        {
            ipSubnetMask[0] = (num >> 24);
            ipSubnetMask[1] = (num >> 16);
            ipSubnetMask[2] = (num >> 8);
            ipSubnetMask[3] = num;
        }

        dhcpRequestType = 0;
        putsUart0("Starting eth0\r\n");
        displayConnectionInfo();
        putsUart0("\r\n");
    }
}

// Function to handle DHCP Messages
void sendDhcpMessage(uint8_t packet[], uint8_t type)
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
        ip->destIp[i]   = 0xFF;
        ip->sourceIp[i] = 0x00;
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 60; // Time-to-Live in seconds
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
    dhcp->xid   = transactionId = htons32(random32());
    dhcp->secs  = 0;

//    dhcp->flags = htons(0x8000); // If bit set to 1 DHCP message should be sent as an IP broadcast using an IP broadcast address (0xFFFFFFFF)
    dhcp->flags = htons(0x0000);

    // Start of adding Options for REQUEST message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = type;

    if((type == 2) || (type == 3))
    {
        // Option 50
        dhcp->options[n++] = 50;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = dhcp->yiaddr[0];
        dhcp->options[n++] = dhcp->yiaddr[1];
        dhcp->options[n++] = dhcp->yiaddr[2];
        dhcp->options[n++] = dhcp->yiaddr[3];

        // Option 54
        dhcp->options[n++] = 54;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = dhcp->siaddr[0];
        dhcp->options[n++] = dhcp->siaddr[1];
        dhcp->options[n++] = dhcp->siaddr[2];
        dhcp->options[n++] = dhcp->siaddr[3];
    }

    if(type == 1)
    {
        // Option 55
        dhcp->options[n++] = 55;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = 1;
        dhcp->options[n++] = 3;
        dhcp->options[n++] = 15;
        dhcp->options[n++] = 6;
    }

    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = 0;
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Set all values in chaddr to 0
    for(i = 0; i < 16; i++)
    {
        dhcp->chaddr[i] = 0;
    }
    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcp->chaddr[i] = macAddress[i];
    }
    // Fill 192 byte data field with zeros
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
    temp16 = ip->protocol;
    sum += (temp16 & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    if(etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize))
    {
        if(type == 1)
        {
            putsUart0("  Tx DHCPDISCOVER\r\n");
        }
    }
}

// Function to handle DHCPREQUEST Messages
// Type 0 = Initial Selection; Type 1 = Bound; Type 2 = Renew; Type 3 = Rebind, Type 4 = INIT-REBOOT
void sendDhcpRequestMessage(uint8_t packet[])
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

    if(dhcpRequestType != 2)
    {
        for (i = 0; i < HW_ADD_LENGTH; i++)
        {
            ether->destAddress[i]   = 0xFF;
            ether->sourceAddress[i] = macAddress[i];
        }
        dhcp->flags = htons(0x8000); // 0x8000 to set Broadcast Flag
    }
    else
    {   for(i = 0; i < HW_ADD_LENGTH; i++)
        {
            ether->destAddress[i]   = serverMacAddress[i];
            ether->sourceAddress[i] = macAddress[i];
        }
        dhcp->flags = htons(0x0000); // 0x0000 to Unicast
    }

    ether->frameType = 0x0008; // For ipv4 (0x0800) big-endian value stored in Frame Type

    // Fill ipFrame
    if(dhcpRequestType == 0) // SELECTING
    {
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            ip->destIp[i]   = 0xFF;
            ip->sourceIp[i] = 0x00;
        }
    }
    else if(dhcpRequestType == 2) // RENEWING
    {
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            ip->destIp[i]   = serverIpAddress[i];
            ip->sourceIp[i] = ipAddress[i];
        }
    }
    else if(dhcpRequestType == 3) // REBINDING
    {
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            ip->destIp[i]   = 0xFF;
            ip->sourceIp[i] = ipAddress[i];
        }
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = htons(1);
    ip->flagsAndOffset = 0;
    ip->ttl            = 60; // Time-to-Live in seconds
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
    dhcp->secs  = 0;

    if(dhcpRequestType != 0)
    {
        dhcp->xid   = htons32(random32());
    }

    // Start of adding Options for REQUEST message
    // Option 53, DHCP Message Type
    n = 0;
    dhcp->options[n++] = 53;
    dhcp->options[n++] = 1;
    dhcp->options[n++] = 3;

    if(dhcpRequestType == 3 || dhcpRequestType == 4)
    {
        // Option 50
        dhcp->options[n++] = 50;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = ipAddress[0];
        dhcp->options[n++] = ipAddress[1];
        dhcp->options[n++] = ipAddress[2];
        dhcp->options[n++] = ipAddress[3];

        // Option 54
        dhcp->options[n++] = 54;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = dhcp->siaddr[0];
        dhcp->options[n++] = dhcp->siaddr[1];
        dhcp->options[n++] = dhcp->siaddr[2];
        dhcp->options[n++] = dhcp->siaddr[3];
    }

    if(dhcpRequestType == 0)
    {
        // Option 50
        dhcp->options[n++] = 50;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = dhcp->yiaddr[0];
        dhcp->options[n++] = dhcp->yiaddr[1];
        dhcp->options[n++] = dhcp->yiaddr[2];
        dhcp->options[n++] = dhcp->yiaddr[3];

        // Option 54
        dhcp->options[n++] = 54;
        dhcp->options[n++] = 4;
        dhcp->options[n++] = dhcp->siaddr[0];
        dhcp->options[n++] = dhcp->siaddr[1];
        dhcp->options[n++] = dhcp->siaddr[2];
        dhcp->options[n++] = dhcp->siaddr[3];
    }

    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;

    // Set CIADDR or Client IP Address
    if(dhcpRequestType == 1 || dhcpRequestType == 2 || dhcpRequestType == 3)
    {
        for(i = 0; i < 4; i++)
        {
            dhcp->ciaddr[i] = ipAddress[i];
        }
    }
    else
    {
        for(i = 0; i < 4; i++)
        {
            dhcp->ciaddr[i] = 0;
        }
    }

    // Set YIADDR and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Set all values in chaddr to 0
    for(i = 0; i < 16; i++)
    {
        dhcp->chaddr[i] = 0;
    }
    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcp->chaddr[i] = macAddress[i];
    }
    // Fill 192 byte data field with zeros
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
    temp16 = ip->protocol;
    sum += (temp16 & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    if(etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize))
    {
        putsUart0("  Tx DHCPREQUEST\r\n");
    }
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
    dhcp->xid   = transactionId;
    dhcp->secs  = 0;

    dhcp->flags = htons(0x0000);

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = ipAddress[i];
        dhcp->yiaddr[i] = 0;
        dhcp->giaddr[i] = 0;
    }

    // Set all values in chaddr to 0
    for(i = 0; i < 16; i++)
    {
        dhcp->chaddr[i] = 0;
    }
    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcp->chaddr[i] = macAddress[i];
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
    if(etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize))
    {
        putsUart0("  Tx DHCPRELEASE\r\n");
    }
}

// DHCPDECLINE Message
void sendDhcpDeclineMessage(uint8_t packet[])
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

    // Set all values in chaddr to 0
    for(i = 0; i < 16; i++)
    {
        dhcp->chaddr[i] = 0;
    }
    // Place MAC Address in top 6 bytes of CHADDR field
    for(i = 0; i < HW_ADD_LENGTH; i++)
    {
        dhcp->chaddr[i] = macAddress[i];
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
    temp16 = ip->protocol;
    sum += (temp16 & 0xFF) << 8;
    etherSumWords(&udp->length, 2);

    // Calculate UDP Header Checksum
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    if(etherPutPacket((uint8_t *)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize))
    {
        putsUart0("  Tx DHCPDECLINE\r\n");
    }
}

// Function to handle DHCPACK message
void getDhcpAckInfo(uint8_t packet[])
{
    uint8_t i = 0;
    uint8_t length = 0;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    etherSetIpAddress(dhcp->yiaddr[0], dhcp->yiaddr[1], dhcp->yiaddr[2], dhcp->yiaddr[3]); // Set IP provided by server

    serverIpAddress[0] = dhcp->siaddr[0];
    serverIpAddress[1] = dhcp->siaddr[1];
    serverIpAddress[2] = dhcp->siaddr[2];
    serverIpAddress[3] = dhcp->siaddr[3];

    serverMacAddress[0] = ether->sourceAddress[0];
    serverMacAddress[1] = ether->sourceAddress[1];
    serverMacAddress[2] = ether->sourceAddress[2];
    serverMacAddress[3] = ether->sourceAddress[3];
    serverMacAddress[4] = ether->sourceAddress[4];
    serverMacAddress[5] = ether->sourceAddress[5];


    while(dhcp->options[i] != 0xFF) // Option 255, END of Messages
   {
        if(dhcp->options[i] == 0x01) // Option 1, Store Subnet Mask
        {
            i += 2;
            ipSubnetMask[0] = dhcp->options[i++];
            ipSubnetMask[1] = dhcp->options[i++];
            ipSubnetMask[2] = dhcp->options[i++];
            ipSubnetMask[3] = dhcp->options[i];
        }
        else if(dhcp->options[i] == 0x03) // Option 3, Store Gateway Address
        {
            i += 2;
            ipGwAddress[0] = dhcp->options[i++];
            ipGwAddress[1] = dhcp->options[i++];
            ipGwAddress[2] = dhcp->options[i++];
            ipGwAddress[3] = dhcp->options[i];
        }
        else if(dhcp->options[i] == 0x06) // Store DNS Address
        {
            i += 2;
            ipDnsAddress[0] = dhcp->options[i++];
            ipDnsAddress[1] = dhcp->options[i++];
            ipDnsAddress[2] = dhcp->options[i++];
            ipDnsAddress[3] = dhcp->options[i];
        }
        else if(dhcp->options[i] == 0x33) // Lease Time
        {
            uint32_t leaseTime = 0;
            i += 2;
            leaseTime |= (dhcp->options[i++] << 24);
            leaseTime |= (dhcp->options[i++] << 16);
            leaseTime |= (dhcp->options[i++] << 8);
            leaseTime |= dhcp->options[i];

            if(dhcpRequestType == 2 || dhcpRequestType == 3)
            {
                stopTimer(renewalTimer);
                stopTimer(rebindTimer);
            }

            startOneShotTimer(renewalTimer,(leaseTime/(2*LEASE_TIME_DIVISOR)));     // Start Renew Timer
            startOneShotTimer(rebindTimer, ((leaseTime*7)/(8*LEASE_TIME_DIVISOR))); // Start Rebind Timer
        }
        else
        {
            length = dhcp->options[++i];
            i += length;
        }
        i++;
    }
}

// Function to check if received packet is DHCP Message
bool etherIsDhcp(uint8_t packet[])
{
    bool dhcpMsg = false;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    if(htons(udp->destPort) == 68 && htons(udp->sourcePort) == 67)
    {
        dhcpMsg = true;
    }

    return dhcpMsg;
}

//
uint8_t dhcpOfferType(uint8_t packet[])
{
    uint8_t i = 0, length, type;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;

    type = 0x00;

    while(dhcp->options[i] != 0xFF) // Option 255, END of Messages
    {

        if(dhcp->options[i] == 0x35) // Option 53, DISCOVER Messages
        {
            i += 2;
            if(dhcp->options[i] == 0x02)
            {
                type = 0x01;
                dhcpRequestType = 0;
                putsUart0("  Rx DHCPOFFER\r\n");
            }
            else if(dhcp->options[i] == 0x05)
            {
                // If dhcpRequestType previously set to renew or rebind then turn off periodic timers
                if(dhcpRequestType == 2)
                {
                    dhcpAckRx = true;
                }
                else if(dhcpRequestType == 3)
                {
                    dhcpAckRx = true;
                }
                type = 0x02;
                putsUart0("  Rx DHCPACK\r\n");
            }
            else if(dhcp->options[i] == 0x06)
            {
                type = 0x03;
                resetAllTimers();
                putsUart0("  Rx DHCPNACK\r\n");
            }
        }
        else if(dhcp->options[i] == 0x33) // Option 51, Lease Time
        {
            length = dhcp->options[++i];
            leaseTime = dhcp->options[i+1];
            i+=length;
        }
        else
        {
            length = dhcp->options[++i];
            i += length;
        }
        i++;
    }
    return type;
}

// Function to store Address in EEPROM
void storeAddressEeprom(uint8_t add1, uint8_t add2, uint8_t add3, uint8_t add4, uint16_t block)
{
    uint32_t num = 0;

    num |= (add1 << 24);
    num |= (add2 << 16);
    num |= (add3 << 8);
    num |= add4;

    writeEeprom(block, num);
}

// Function "erases" perviously stored values in EEPROM
void eraseAddressEeprom()
{
    writeEeprom(0x0010, 0xFFFFFFFF); // DHCP Mode
    writeEeprom(0x0011, 0xFFFFFFFF); // IP
    writeEeprom(0x0012, 0xFFFFFFFF); // GW
    writeEeprom(0x0013, 0xFFFFFFFF); // DNS
    writeEeprom(0x0014, 0xFFFFFFFF); // SN
}
