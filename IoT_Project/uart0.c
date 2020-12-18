// uart0.c
// William Bozarth
// Created on: October 7, 2020

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "uart0.h"

UART0_BUFFER uart0Info = {0};

// Initialize UART0
void initUart0(uint32_t baudRate, uint32_t fcyc)
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    _delay_cycles(3);

    enablePort(PORTA);
    _delay_cycles(3);

    // Configure UART0 pins
    selectPinPushPullOutput(UART0_TX);
    selectPinDigitalInput(UART0_RX);
    setPinAuxFunction(UART0_TX, GPIO_PCTL_PA1_U0TX);
    setPinAuxFunction(UART0_RX, GPIO_PCTL_PA0_U0RX);

    // Configure UART0 with default baud rate
    UART0_CTL_R = 0;                 // turn-off UART0 to allow safe programming
    UART0_CC_R  = UART_CC_CS_SYSCLK; // use system clock (usually 40 MHz)

    // Setup UART0 Baud Rate
    setUart0BaudRate(baudRate, fcyc);
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate;   // calculate divisor (r) in units of 1/128,
                                                        // where r = fcyc / 16 * baudRate
    UART0_CTL_R  = 0;                                   // turn-off UART0 to allow safe programming
    UART0_IBRD_R = divisorTimes128 >> 7;                // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128 + 1) >> 1) & 63;   // set fractional value to round(fract(r)*64)
    UART0_LCRH_R = UART_LCRH_WLEN_8;                    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R  = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN; // turn-on UART0
    UART0_IM_R   = UART_IM_TXIM;                        // turn-on TX interrupt
    NVIC_EN0_R   |= 1 << (INT_UART0-16);                // turn-on interrupt 21 (UART0)
}


// Returns the status of the receive buffer
bool kbhitUart0(void)
{
    return !(UART0_FR_R & UART_FR_RXFE);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0(void)
{
    return UART0_DR_R & 0xFF; // get character from fifo
}

// Add characters to UART0 TX FIFO
void sendUart0String(char str[])
{
    int i = 0;

    // Write string to Tx Ring Buffer
    while(str[i] != '\0')
    {
        // If NOT full write next Character
        if(!(fullRingBuffer()))
        {
            writeToQueue(str[i]);
            i++;
        }
    }

    UART0_ICR_R = 0x00000FFF;

    // Check to see if UART Tx holding register is empty
    if(UART0_FR_R & UART_FR_TXFE  && !(emptyRingBuffer()))
        UART0_DR_R = readFromQueue(); // "Prime Pump" by writing 1st char to Uart0
}

// Add characters to UART0 TX FIFO
void sendUart0StringLiteral(const char str[])
{
    int i = 0;

    // Write string to Tx Ring Buffer
    while(str[i] != '\0')
    {
        // If NOT full write next Character
        if(!(fullRingBuffer()))
        {
            writeToQueue(str[i]);
            i++;
        }
    }

    UART0_ICR_R = 0x00000FFF;

    // Check to see if UART Tx holding register is empty
    if(UART0_FR_R & UART_FR_TXFE  && !(emptyRingBuffer()))
        UART0_DR_R = readFromQueue(); // "Prime Pump" by writing 1st char to Uart0
}

// Update current position of writeIndex variable for Ring Buffer
void writeToQueue(char c)
{
    uart0Info.uart0String[uart0Info.writeIndex] = c;
    uart0Info.writeIndex = (uart0Info.writeIndex + 1) % QUEUE_BUFFER_LENGTH;
}

// Update current position of readIndex variable for Ring Buffer
char readFromQueue(void)
{
    char c;
    c = uart0Info.uart0String[uart0Info.readIndex];
    uart0Info.readIndex = (uart0Info.readIndex + 1) % QUEUE_BUFFER_LENGTH;
    return c;
}

// Returns true if ring buffer is EMPTY
bool emptyRingBuffer(void)
{
    if(uart0Info.writeIndex == uart0Info.readIndex)
        return true;

    return false;
}

// Returns true if ring buffer is FULL
bool fullRingBuffer(void)
{
    if(((uart0Info.writeIndex + 1) % QUEUE_BUFFER_LENGTH) == uart0Info.readIndex)
        return true;

    return false;
}

// Function to Print Main Menu
void printMainMenu(void)
{
    // Output to terminal configuration info
    displayConnectionInfo();

    sendUart0String("\r\nCommands:\r\n");
    sendUart0String("  dhcp ON|OFF|REFRESH|RELEASE\r\n");
    sendUart0String("  set IP|GW|DNS|SN|MQTT w.x.y.z\r\n");
    sendUart0String("  ifconfig\r\n");
    sendUart0String("  publish TOPIC DATA\r\n");
    sendUart0String("  subscribe TOPIC\r\n");
    sendUart0String("  unsubscribe TOPIC\r\n");
    sendUart0String("  connect\r\n");
    sendUart0String("  disconnect\r\n");
    sendUart0String("  help Inputs\r\n");
    sendUart0String("  help Outputs\r\n");
    sendUart0String("  help Subs\r\n");
    sendUart0String("  reboot\r\n\r\n");
}

// Function to Print Help Inputs
void printHelpInputs(void)
{
    sendUart0String("\r\n  Local Input Topics to MQTT Client:\r\n");
    sendUart0String("    env/pb\r\n");
    sendUart0String("    env/temp\r\n");
    sendUart0String("    env/led/green on|off\r\n");
    sendUart0String("    env/led/red   on|off\r\n");
    sendUart0String("    env/led/blue  on|off\r\n");
}

// Function to Print Help Outputs
void printHelpOututs(void)
{
    sendUart0String("\r\n  Local Output Topics to MQTT Client:\r\n");
    sendUart0String("    env/uart\r\n");
    sendUart0String("    env/led/green STATUS\r\n");
    sendUart0String("    env/led/red   STATUS\r\n");
    sendUart0String("    env/led/blue  STATUS\r\n");
}

// Handle UART0 Interrupts
void uart0Isr(void)
{
    // Writing a 1 to the bits in this register clears the bits in the UARTRIS and UARTMIS registers
    UART0_ICR_R = 0xFFF;

    // Check to see if UART Tx holding register is empty and send next byte of data
    if((UART0_FR_R & UART_FR_TXFE) && !(emptyRingBuffer()))
        UART0_DR_R = readFromQueue();
}
