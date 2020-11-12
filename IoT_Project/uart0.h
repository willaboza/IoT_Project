// uart0.h
// William Bozarth
// Created on: October 7, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef UART0_H_
#define UART0_H_

#include <stdint.h>
#include <stdbool.h>
#include "uart0.h"

//
// Pins and Defines
//
#define UART0_TX PORTA,1
#define UART0_RX PORTA,0
#define QUEUE_BUFFER_LENGTH 80

//
// Structure Definition
//
typedef struct _UART0_BUFFER
{
    char uart0String[QUEUE_BUFFER_LENGTH];
    uint8_t writeIndex;
    uint8_t readIndex;
} UART0_BUFFER;

extern UART0_BUFFER uart0Info;

//
// Subroutines
//
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void writeToQueue(char c);
bool kbhitUart0(void);
char getcUart0(void);
void initUart0(void);
char readFromQueue(void);
bool emptyRingBuffer(void);
bool fullRingBuffer(void);
void printMainMenu(void);
void printHelpInputs(void);
void printHelpOututs(void);
void sendUart0String(char str[]);
void sendUart0StringLiteral(const char str[]);
void newLine(void);
void uart0Isr(void);

#endif /* UART0_H_ */
