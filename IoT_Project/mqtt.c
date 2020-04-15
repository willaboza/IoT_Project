/*
 * mqtt.c
 *
 *  Created on: Apr 14, 2020
 *      Author: willi
 */

#include "mqtt.h"

uint8_t ipMqttAddress[IP_ADD_LENGTH] = {0};

// Set DNS Address
void setMqttAddress(uint8_t mqtt0, uint8_t mqtt1, uint8_t mqtt2, uint8_t mqtt3)
{
    ipMqttAddress[0] = mqtt0;
    ipMqttAddress[1] = mqtt1;
    ipMqttAddress[2] = mqtt2;
    ipMqttAddress[3] = mqtt3;
}

// Get DNS Address
void getMqttAddress(uint8_t mqtt[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        mqtt[i] = ipMqttAddress[i];
}
