# IoT Project

This project was broken up into two seperate parts.

## Part 1 Overview:
  
  The goal of part 1 is to build the essential parts of a simple embedded ethernet device by providing a DHCP client and a TCP server. 
  
  A Texas Instruments (TI) TM4C123GH6PM microcontroller is used along with an ENC28J60 for interfacing with ethernet.
  
## Part 2 Overview: 
  
  Add IoT support to Part 1 by implementing an MQTT client with the ability to publish and subscribe to topics on an MQTT broker. Mosquitto was used for this project to implement the MQTT broker.

## Command Line Interface Requirements

   Teraterm is used as a virtual COM port to interface with the microcontroller, over UART0, allowing for transmition/reception of information between the user and device.
   
   | Command | Description |
   | :----: | :----: |
   | dhcp ON/OFF | Enables and disables DHCP mode (Mode is stored persistently in EEPROM). |
   | dhcp Refresh/Release | Refreshes current IP address or releases current IP address (If in DHCP mode).|
   | set IP/GW/DNS/SN w.x.y.z | Used to set the IP, Gatewat, DNS, and Subnet Mask addresses when DHCP mode is disabled (Values stored persistently in EEPROM). |
   | ifconfig | Displays current IP, SN, GW, and DNS addresses as well as current DHCP mode. |
   | set MQTT w.x.y.z | Sets IP address of MQTT broker (Stored persistently in EEPROM) |
   | publish TOPIC DATA | Used to publish a topic and its associated data to MQTT broker |
   | subscribe TOPIC | Subscribes to topic and displays data to terminal when topic is received later. |
   | unsubscribe TOPIC | Unsubscribe from a topic. |
   | connect | Sends a message to MQTT broker to connect. |
   | disconnect | Sends a message to MQTT broker to disconnect. |
   | reboot | Restarts microcontroller |
