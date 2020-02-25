/*
 * dhcp.c
 *
 *  Created on: Feb 12, 2020
 *      Author: William Bozarth
 */

#include "dhcp.h"

// See if DHCP mode is ON or OFF
bool usingDhcp()
{
    bool dhcpMode = false;

    return dhcpMode;
}

// Function to send DHCP message
// uint8_t type is the option 53 argument
// ipAdd is the address needed to send
//
void sendDhcpDiscoverMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[])
{
    uint8_t i;
    uint32_t dhcpSize;

    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame    *ip    = (ipFrame*)&ether->data;
    ip->revSize       = htons(0x45); // Four-bit version field is 4 and IHL is 5 indicating the size is 20 bytes
    udpFrame   *udp   = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame  *dhcp  = (dhcpFrame*)&udp->data;

    // Fill etherFrame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i]   = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }

    // Fill ipFrame
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i]   = 0xFF;
        ip->sourceIp[i] = 0;
    }

    ip->length = htons(((ip->revSize & 0xF) * 4) + 8); // adjust lengths

    // Fill UDP Frame
    udp->destPort   = htons(0x43); // UDP port # 67 (0x43h) is the destination port of a server
    udp->sourcePort = htons(0x44); // UDP port # 68 (0x44h) is used by the client
    udp->length     = htons(8);

    // Fill DHCP Frame
    dhcp->op    = htons(1);
    dhcp->htype = htons(1);
    dhcp->hlen  = htons(6);
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

    for(i = 0; i < 16; i++)
    {
        dhcp->chaddr[i] = 0;
    }

    for(i = 0; i < 16; i++)
    {
        dhcp->data[i] = 0;
    }

    dhcp->magicCookie = htons(0x63825363);
    dhcp->options[0]  = htons(type);

    dhcpSize = (uint32_t)(sizeof(dhcpFrame) + sizeof(type)); // Calculate size of DHCP header

    // Calculate IP Header Checksum
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();

    // Calculate UDP Header Checksum

    // send packet with size = ether + udp header + ip header + udp_size + dchp header + options
    etherPutPacket((uint8_t*)ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize);
}

//
void getDhcpOfferMessage(uint8_t packet[])
{
    // IP header Encapsulation
    etherFrame *ether = (etherFrame*)packet;
    ipFrame    *ip    = (ipFrame*)&ether->data;
    ip->revSize       = 0x45; // Four-bit version field is 4 and IHL is 5 indicating the size is 20 bytes
    udpFrame   *udp   = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame  *dhcp  = (dhcpFrame*)&udp->data;
}
