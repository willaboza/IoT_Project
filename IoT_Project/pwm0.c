/*
 * pwm0.c
 *
 *  Created on: May 1, 2020
 *      Author: William Bozarth
 */

#include "pwm0.h"

void initPwm0()
{
    // Enable clocks
    SYSCTL_RCGC0_R |= SYSCTL_RCGC0_PWM0;
    _delay_cycles(3);

    // Configure PWM0 pins
    selectPinPushPullOutput(PWM0_RED_LED);
    selectPinPushPullOutput(PWM0_BLUE_LED);
    selectPinPushPullOutput(PWM0_GREEN_LED);

    setPinAuxFunction(PWM0_RED_LED, GPIO_PCTL_PF1_M1PWM5);
    setPinAuxFunction(PWM0_BLUE_LED, GPIO_PCTL_PF2_M1PWM6);
    setPinAuxFunction(PWM0_GREEN_LED, GPIO_PCTL_PF3_M1PWM7);

    SYSCTL_RCGCPWM_R = 0x2;

    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R1;                // reset PWM0 module
    SYSCTL_SRPWM_R = 0;                              // leave reset state
    PWM1_2_CTL_R = 0;                                // turn-off PWM0 generator 1
    PWM1_3_CTL_R = 0;                                // turn-off PWM0 generator 2

    // output 1 on PWM0, gen 0b, cmpb
    PWM1_2_GENB_R = PWM_2_GENB_ACTCMPBD_ZERO | PWM_2_GENB_ACTLOAD_ONE;
    // output 2 on PWM0, gen 1a, cmpa
    PWM1_3_GENA_R = PWM_3_GENA_ACTCMPAD_ZERO | PWM_3_GENA_ACTLOAD_ONE;
    // output 3 on PWM0, gen 1b, cmpb
    PWM1_3_GENB_R = PWM_3_GENB_ACTCMPBD_ZERO | PWM_3_GENB_ACTLOAD_ONE;

    // set period to 40 MHz sys clock / 2 / 1024 = 19.53125 kHz
    PWM1_2_LOAD_R = 1024;
    PWM1_3_LOAD_R = 1024;

    // invert outputs for duty cycle increases with increasing compare values
    PWM1_2_CMPB_R = 1023;                               // red off (0=always low, 1023=always high)
    PWM1_3_CMPB_R = 1023;                               // green off
    PWM1_3_CMPA_R = 1023;                               // blue off

    PWM1_2_CTL_R = PWM_2_CTL_ENABLE;                 // turn-on PWM0 generator 1
    PWM1_3_CTL_R = PWM_3_CTL_ENABLE;                 // turn-on PWM0 generator 2
    PWM1_ENABLE_R = PWM_ENABLE_PWM5EN | PWM_ENABLE_PWM6EN | PWM_ENABLE_PWM7EN;// enable outputs

}

//Set Red,Green,and Blue LED Colors
void setRgbColor(uint16_t red, uint16_t blue, uint16_t green)
{
    PWM1_2_CMPB_R = red;      //set value recorded for red
    PWM1_3_CMPA_R = blue;     //set value recorded for blue
    PWM1_3_CMPB_R = green;    //set value recorded for green
}

// Change value of red LED
void setRedLed(uint16_t red)
{
    PWM1_2_CMPB_R = red;      //set value recorded for red
}

// Change value of green LED
void setGreenLed(uint16_t green)
{
    PWM1_3_CMPB_R = green;    //set value recorded for green
}

// Change value of blue LED
void setBlueLed(uint16_t blue)
{
    PWM1_3_CMPA_R = blue;     //set value recorded for blue
}

//return measured value in the range of 0... 255 using
// m = (m/th)*(tMax-tMin), where th = threshold, tMax
//and tMin is target max and min, and m = measured value
// Result = ((Input - Input_Low)/(Input_High - Input_Low)) * (Output_High - Output_low) + Output_Low
int normalizeRgbColor(int measurement)
{
    uint16_t result;
    float numerator, denominator, ratio;
//    char str[50];

//    sprintf(str, "  %u.0\0", measurement);
//    sendUart0String(str);
    /*
    snprintf(buffer, sizeof(buffer), "%u.0\0",measurement);
    numerator = atof(buffer);
    snprintf(buffer, sizeof(buffer), "%u.0\0",threshold);
    denominator = atof(buffer);
    ratio = (numerator/denominator) * 255.0;
    result = ratio;
    */
    if(result > 255)
    {
        result = 255;
    }

    return result;
}

// Result = ((Input - Input_Low)/(Input_High - Input_Low)) *
// (Output_High - Output_low) + Output_Low
int scaleRgbColor()
{



}



