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

#define MQTT_KEEP_ALIVE_TIME 180  // Keep Alive time for MQTT in seconds
#define MQTT_TIME_TO_LIVE    60
#define MQTT_MAX_BUFFER_SIZE 125
#define MQTT_MAX_FIELD_SIZE  8
#define MQTT_MAX_TABLE_SIZE  10
#define MQTT_MAX_SUB_CHARS   25
#define MQTT_ADD_LENGTH      4
#define MQTT_BROKER_PORT     1883
#define MQTT_HW_ADD_LENGTH   6

extern uint8_t mqttIpAddress[MQTT_ADD_LENGTH];
extern uint8_t mqttMacAddress[MQTT_HW_ADD_LENGTH];
extern uint8_t mqttMsgType;
extern uint16_t mqttSrcPort;
extern uint16_t mqttPacketId;

typedef enum
{
    RESERVED,
    CONNECT,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT
} mqttPacketTypes;

typedef struct _mqttFrame
{
  uint8_t control;      // 1st 4 MSBs are Packet Type and other 4 bits are Control Flags
  uint8_t packetLength; // Bit 7 = Continuation Flag, bits 6-3 =
  uint8_t data[0];      //
} mqttFrame;

typedef struct _mqttTopics
{
    bool validBit;
    char subs[MQTT_MAX_SUB_CHARS];
} mqttTopics;

extern mqttTopics topics[MQTT_MAX_TABLE_SIZE];

uint8_t getMqttMsgType(uint8_t packet[]);
void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3);
void getMqttAddress(uint8_t mqtt[]);
bool isMqttMessage(uint8_t packet[]);
void sendMqttConnectMessage(uint8_t packet[], uint16_t flags);
void mqttConnectAckMessage(uint8_t packet[]);
void sendMqttDisconnectMessage(uint8_t packet[] , uint16_t flags);
void sendMqttPingRequest(uint8_t packet[], uint16_t flags);
void mqttPubAckRec(uint8_t packet[], uint8_t type, uint16_t flags, uint16_t packetId);
void sendMqttPublish(uint8_t packet[], uint16_t flags, char topic[], char data[]);
void mqttSubscribe(uint8_t packet[], uint16_t flags, char topic[]);
void mqttUnsubscribe(uint8_t packet[], uint16_t flags, char topic[]);
void createEmptySlot(char info[]);
uint8_t findEmptySlot(void);
void mqttMessageEstablished(void);
void mqttPingTimerExpired(void);

#endif /* MQTT_H_ */
