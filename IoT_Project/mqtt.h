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

extern uint8_t ipMqttAddress[IP_ADD_LENGTH];

void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3);
void getMqttAddress(uint8_t mqtt[4]);

#endif /* MQTT_H_ */
