/*
 * dhcp.c
 *
 *  Created on: Feb 12, 2020
 *      Author: William Bozarth
 */

#include "dhcp.h"

// Function to determine if DHCP mode ENABLED or DISABLED
void readDeviceConfig()
{
    uint32_t eepromValue;

    if(readEeprom(1) != 0xFFFFFFFF)
    {
        // Store 32 bits of Ip address retrieved from EEPROM
        // in uint32 variable. Then right shift desired 8-bit address value
        // into the 8 LSB and store in uint8 variables for use in setting addresses.
        eepromValue = readEeprom(1);
        ipAddress[0] = eepromValue >> 24;
        ipAddress[1] = eepromValue >> 16;
        ipAddress[2] = eepromValue >> 8;
        ipAddress[3] = eepromValue;

        eepromValue = readEeprom(2);
        ipSubnetMask[0] = eepromValue >> 24;
        ipSubnetMask[1] = eepromValue >> 16;
        ipSubnetMask[2] = eepromValue >> 8;
        ipSubnetMask[3] = eepromValue;

        eepromValue = readEeprom(3);
        ipGwAddress[0] = eepromValue >> 24;
        ipGwAddress[1] = eepromValue >> 16;
        ipGwAddress[2] = eepromValue >> 8;
        ipGwAddress[3] = eepromValue;

        eepromValue = readEeprom(4);
        ipDnsAddress[0] = eepromValue >> 24;
        ipDnsAddress[1] = eepromValue >> 16;
        ipDnsAddress[2] = eepromValue >> 8;
        ipDnsAddress[3] = eepromValue;

        eepromValue = readEeprom(5);
        macAddress[0] = eepromValue >> 24;
        macAddress[1] = eepromValue >> 16;
        macAddress[2] = eepromValue >> 8;
        macAddress[3] = eepromValue;
        eepromValue = readEeprom(6);
        macAddress[4] = eepromValue >> 8;
        macAddress[5] = eepromValue;

        displayConnectionInfo();
        putsUart0("\r\n");
        etherEnableDhcpMode();
    }
    else
    {
        putsUart0("Starting eth0\r\n");
        initEthernetInterface();
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

    if(type != 7)
    {
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
    }
    else if(type == 7)
    {
        // Fill etherFrame
        for (i = 0; i < HW_ADD_LENGTH; i++)
        {
            ether->destAddress[i]   = serverMacAddress[i];
            ether->sourceAddress[i] = macAddress[i];
        }

        ether->frameType = 0x0008; // For ipv4 (0x0800) big-endian value stored in Frame Type

        // Fill ipFrame
        for (i = 0; i < IP_ADD_LENGTH; i++)
        {
            ip->destIp[i]   = serverIpAddress[i];
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
    dhcp->xid   = 0; // Potential XID: 0x05066A00
    dhcp->secs  = 0;

    if(type != 7)
    {
        dhcp->flags = htons(0x8000); // If bit set to 1 DHCP message should be sent as an IP broadcast using an IP broadcast address (0xFFFFFFFF)
    }
    else
    {
        dhcp->flags = htons(0x0000);
    }
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

    // Option 255 specifies end of DHCP options field
    dhcp->options[n++] = 255;

    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    if()
    {
        for(i = 0; i < 4; i++)
        {
            dhcp->ciaddr[i] = 0;
        }
    }
    else
    {
        for(i = 0; i < 4; i++)
        {
            dhcp->ciaddr[i] = 0;
        }
    }
    // Set CIADDR, YIADDR, and GIADDR to 0.0.0.0
    for(i = 0; i < 4; i++)
    {
//        dhcp->ciaddr[i] = 0;
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
    // etherSumWords(&ip->revSize, 10);
    // etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    // ip->headerChecksum = getEtherChecksum();
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
            putsUart0("  DHCPDISCOVER Message Tx.\r\n");
        }
        else if(type == 3)
        {
            putsUart0("  DHCPREQUEST Message Tx.\r\n");
        }
        else if(type == 4)
        {
            putsUart0("  DHCPDECLINE Message Tx.\r\n");
        }
        else if(type == 7)
        {
            putsUart0("  DHCPRELEASE Message Tx.\r\n");
        }
    }
}

// Function to handle DHCPACK message
void setDhcpAckInfo(uint8_t packet[])
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
        if(dhcp->options[i] == 0x01) // Option 1, Subnet Mask Option
        {
            i += 2;
            ipSubnetMask[0] = dhcp->options[i++];
            ipSubnetMask[1] = dhcp->options[i++];
            ipSubnetMask[2] = dhcp->options[i++];
            ipSubnetMask[3] = dhcp->options[i];
        }
        else if(dhcp->options[i] == 0x03) // Option 3, Gateway Address Option
        {
            i += 2;
            ipGwAddress[0] = dhcp->options[i++];
            ipGwAddress[1] = dhcp->options[i++];
            ipGwAddress[2] = dhcp->options[i++];
            ipGwAddress[3] = dhcp->options[i];
        }
        else if(dhcp->options[i] == 0x06)
        {
            i += 2;
            ipDnsAddress[0] = dhcp->options[i++];
            ipDnsAddress[1] = dhcp->options[i++];
            ipDnsAddress[2] = dhcp->options[i++];
            ipDnsAddress[3] = dhcp->options[i];
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
                putsUart0("  DHCPOFFER Message Rx.\r\n");
            }
            else if(dhcp->options[i] == 0x05)
            {
                type = 0x02;
                putsUart0("  DHCPACK Message Rx.\r\n");
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
