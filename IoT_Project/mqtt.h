/*
 * mqtt.h
 *
 *  Created on: Apr 14, 2020
 *      Author: willi
 */

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "ethernet.h"
#include "tcp.h"
#include "mosquitto.h"

typedef struct _mqttFrame
{
  uint8_t   control; // 1st 4 MSBs are Packet Type and other 4 bits are Control Flags
  uint8_t   packetLength;  // Bit 7 = Continuation Flag, bits 6-3 =
  uint8_t   data[0];
} mqttFrame;

typedef struct _loginInfo
{
    char userName[25];
    char userPassword[25];
    uint16_t clientIdLength;
    char clientId[25];

} loginInfo;

extern uint8_t ipMqttAddress[IP_ADD_LENGTH];
extern bool mqttMsgFlag;
extern uint8_t mqttMsgType;

void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3);
void getMqttAddress(uint8_t mqtt[4]);
bool etherIsMqttRequest(uint8_t packet[]);
void sendMqttTcpSyn(uint8_t packet[], uint16_t flags);
void mqttConnectMessage(uint8_t packet[], uint16_t flags);
void mqttConnectAckMessage(uint8_t packet[]);
void mqttDisconnectMessage(uint8_t packet[] , uint16_t flags);

#endif /* MQTT_H_ */
