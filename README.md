# IoT Project

This project was broken up into two seperate parts.

## Part 1 Overview:
  
  The goal of part 1 is to build the essential parts of a simple embedded ethernet device by providing a DHCP client and a TCP server. 
  
  A Texas Instruments (TI) TM4C123GH6PM microcontroller is used along with an ENC28J60 for interfacing with ethernet.
  
## Part 2 Overview: 
  
  Add IoT support to Part 1 by implementing an MQTT client with the ability to publish and subscribe to topics on an MQTT broker. Mosquitto was used for this project to implement the MQTT broker.

## Command Line Interface Requirements

   Universal Asynchronous Receiver Transmitter (UART) will be used to interface with the device by using UART0 on the microcontroller. A virtual COM port, using Teraterm, provides a 
