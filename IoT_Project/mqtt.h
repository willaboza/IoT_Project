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
#include "terminal.h"
#include "tcp.h"
#include "uart.h"

#define KEEP_ALIVE_TIME 60 // Keep Alive time for MQTT in seconds

typedef struct _mqttFrame
{
  uint8_t   control; // 1st 4 MSBs are Packet Type and other 4 bits are Control Flags
  uint8_t   packetLength;  // Bit 7 = Continuation Flag, bits 6-3 =
  uint8_t   data[0];
} mqttFrame;

extern uint8_t ipMqttAddress[IP_ADD_LENGTH];
extern uint8_t mqttMsgType;
extern uint16_t packetId;
extern uint16_t mqttSrcPort;
extern bool mqttMsgFlag;
extern bool sendPubackFlag;
extern bool sendPubrecFlag;
extern bool sendMqttConnect;
extern bool pubMessageReceived;

void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3);
void getMqttAddress(uint8_t mqtt[4]);
bool mqttMessage(uint8_t packet[]);
void mqttConnectMessage(uint8_t packet[], uint16_t flags);
void mqttConnectAckMessage(uint8_t packet[]);
void mqttDisconnectMessage(uint8_t packet[] , uint16_t flags);
void mqttPingRequest(uint8_t packet[], uint16_t flags);
void processMqttMessage(uint8_t packet[], char topic[], char data[]);
void mqttPubAckRec(uint8_t packet[], uint16_t flags, uint8_t type);
void mqttPublish(uint8_t packet[], uint16_t flags, char topic[], char data[]);
void mqttSubscribe(uint8_t packet[], uint16_t flags, char topic[]);
void mqttUnsubscribe(uint8_t packet[], uint16_t flags, char topic[]);

#endif /* MQTT_H_ */
