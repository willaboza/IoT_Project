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

}

// Send Broadcast to see who is on the network
void broadcastDhcp()
{

}

void receiveOffer()
{

}

// Function to send DHCP message
void sendDhcpMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[])
{
    uint32_t sum;
}

bool getAck(){

}

// IP header Encapsulation
//etherFrame *ether = (etherFrame*)packet;
//ipFrame    *ip = (ipFrame*)&ether->data;
//udpFrame   *udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
//dhcpFrame  *dhcp = (dhcpFrame*)&udp->data;
