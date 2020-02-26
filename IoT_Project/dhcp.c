/*
 * dhcp.c
 *
 *  Created on: Feb 12, 2020
 *      Author: William Bozarth
 */

#include "dhcp.h"

// Function to send DHCP message
// uint8_t type is the option 53 argument
// ipAdd is the address needed to send
void sendDhcpDiscoverMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[])
{
    uint8_t i;
    uint16_t temp16;
    uint32_t dhcpSize;

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
    ether->frameType = htons(0x0800); // For ipv4

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = 0xFF;
        ip->sourceIp[i] = ipAdd[i];
    }

    // For purposes of computing the checksum, the value of the checksum field is zero. (See RFC791 Section 3.1)
    ip->headerChecksum = 0;
    ip->typeOfService  = 0;
    ip->id             = 0;
    ip->flagsAndOffset = 0;
    ip->ttl            = 10; // Time-to-Live in seconds
    ip->protocol       = 17;  // UDP = 17 or 0x21h (See RFC790 Assigned Internet Protocol Numbers table for list)

    // Fill UDP Frame
    udp->destPort   = htons(67); // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = htons(68); // UDP port # 68 (0x44h) is used by the client
    udp->check      = 0;         // Set to 0 before performing

    // Fill DHCP Frame
    dhcp->op    = 1;
    dhcp->htype = 1;
    dhcp->hlen  = 6;
    dhcp->hops  = 0;
    dhcp->xid   = 0;
    dhcp->secs  = 0;
    dhcp->flags = 0;

    for(i = 0; i < 4; i++)
    {
        dhcp->ciaddr[i] = 0;
        dhcp->yiaddr[i] = 0;
        dhcp->siaddr[i] = 0;
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

    for(i = 0; i < 192; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = 0x63825363;
    dhcp->options[0]  = type;

    // Calculate size of DHCP header
    dhcpSize = (sizeof(dhcpFrame) + sizeof(type));

    // Calculate IP Header Checksum
    sum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + dhcpSize); // adjust length of IP header
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
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
    etherPutPacket((uint8_t*)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);
}

//
void getDhcpOfferMessage(uint8_t packet[])
{
    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame *ip       = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit version field is 4 and IHL is 5 indicating the size is 20 bytes
    udpFrame *udp     = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame *dhcp   = (dhcpFrame*)&udp->data;
}
