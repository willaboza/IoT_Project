// mqtt.h
// William Bozarth
// Created on: April 14, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef MQTT_H_
#define MQTT_H_

#include "mqtt.h"

#define KEEP_ALIVE_TIME 120 // Keep Alive time for MQTT in seconds
#define MAX_BUFFER_SIZE 80
#define MAX_FIELD_SIZE  8
#define MAX_TABLE_SIZE  10
#define MAX_SUB_CHARS   25
#define MQTT_ADD_LENGTH 4

typedef struct _mqttFrame
{
  uint8_t   control;      // 1st 4 MSBs are Packet Type and other 4 bits are Control Flags
  uint8_t   packetLength; // Bit 7 = Continuation Flag, bits 6-3 =
  uint8_t   data[0];
} mqttFrame;

typedef struct _subscribedTopics
{
    bool validBit;
    char subs[MAX_SUB_CHARS];
} subscribedTopics;

extern subscribedTopics topics[MAX_TABLE_SIZE];
extern uint8_t ipMqttAddress[MQTT_ADD_LENGTH];
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
void mqttPubAckRec(uint8_t packet[], uint16_t flags, uint8_t type);
void mqttPublish(uint8_t packet[], uint16_t flags, char topic[], char data[]);
void mqttSubscribe(uint8_t packet[], uint16_t flags, char topic[]);
void mqttUnsubscribe(uint8_t packet[], uint16_t flags, char topic[]);
void createEmptySlot(char info[]);
uint8_t findEmptySlot(void);

#endif /* MQTT_H_ */
