/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "pomodoro.h"

uint32_t secondsSinceStart;
uint32_t periodIncrement;

Pomodoro PomoTimer;
PulseConfig PulseData;

GPIOWrapper ledArray[NUMBER_OF_LEDS];

// LUT to normalize LED brightness
uint8_t ledLookupTable[] = {0,0,1,1,1,2,2,3,3,3,4,4,5,5,6,6,6,7,7,8,8,8,9,9,10,10,11,11,11,12,12,13,13,14,14,15,15,15,16,16,17,17,18,18,19,19,20,20,21,21,21,22,22,23,23,24,24,25,25,26,26,27,27,28,28,29,29,30,30,31,31,32,32,33,34,34,35,35,36,36,37,37,38,38,39,40,40,41,41,42,42,43,44,44,45,45,46,46,47,48,48,49,49,50,51,51,52,53,53,54,54,55,56,56,57,58,58,59,60,60,61,62,62,63,64,65,65,66,67,67,68,69,70,70,71,72,73,73,74,75,76,76,77,78,79,80,80,81,82,83,84,84,85,86,87,88,89,90,91,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,119,120,121,122,123,124,126,127,128,129,131,132,133,135,136,137,139,140,141,143,144,146,147,149,150,152,153,155,156,158,160,161,163,165,166,168,170,172,174,175,177,179,181,183,185,187,190,192,194,196,199,201,203,206,208,211,214,216,219,222,225,228,231,234,237,241,244,248,252,255};

uint8_t count;
uint8_t mb = MAXBRIGHTNESS;

int main(void)
{
    SYSCFG_DL_init();

    //have to enable interrupt (from academy)
    NVIC_EnableIRQ(SECOND_TICKER_INST_INT_IRQN);
    NVIC_EnableIRQ(PULSE_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(PWM_0_INST_INT_IRQN);
    NVIC_EnableIRQ(GPIO_GRP_0_INT_IRQN); // for the button 

    // global brightness value 


    //DL_GPIO_setPins(ledArray[0].gpioRegs, ledArray[0].pinNumber);


    /*
    DL_GPIO_setPins(GPIOA, GPIO_GRP_0_REAL_LED_0_PIN |
		GPIO_GRP_0_REAL_LED_1_PIN |
		GPIO_GRP_0_REAL_LED_2_PIN |
		GPIO_GRP_0_REAL_LED_3_PIN);


    DL_GPIO_setPins(GPIOB, GPIO_GRP_0_REAL_LED_4_PIN);
    */


    

    // initialize the array of LEDs for the pomodoro timer
    // ledArray is already a pointer - no need to dereference
    initLedArray(ledArray, NUMBER_OF_LEDS);

        for(int i = 0; i < NUMBER_OF_LEDS; i++){
        DL_GPIO_setPins(ledArray[i].gpioRegs, ledArray[i].pinNumber);
    }

    

    // initialize values for pomodoro timer
    initPomoTimer(&PomoTimer);
    
    //initialize pulseConfig with correct values (this will be a method eventually)
    initPulseConfig(&PulseData, mb);

    //this part turns the light off

    while (1) {
        __WFI();
    }

    
    
}

uint32_t initPomoTimer(Pomodoro *ptrPomoTimer){
    /*
    This function initializes the pomodoro timer. It should be re-invokable to reset for another session. 
    */
    uint32_t activePeriod = NUMBER_OF_LEDS; 
    activePeriod -=1; // have to do 2 lines b/c macro doesn't play nice with operators

    // note that activePeriod is 0 - indexed
    ptrPomoTimer->activePeriod = activePeriod;

    // make this mask by generating 2^(NUMBER_OF_LEDS), then subtracting 1. mask of all np x 1.
    // existing is a uint32. up to 32 LEDs supported.
    ptrPomoTimer->existingLEDMask = (1 << (activePeriod + 1)) - 1;
    // when we start, all LEDs are on
    ptrPomoTimer->litLEDMask =  ptrPomoTimer->existingLEDMask;
    // only one LED "active". This means it will pulse and we'll keep track of time here.
    // default it to the highest # led.
    ptrPomoTimer->activePeriodLEDMask = 1 << activePeriod;

    // set LEDs to on
    updateLedStatuses(ptrPomoTimer, ledArray, NUMBER_OF_LEDS);

    // start seconds counter for Pomodoro timer
    DL_Timer_startCounter(SECOND_TICKER_INST);

    // start timer for LED pulse 
    DL_Timer_startCounter(PULSE_TIMER_INST);

    // start global PWM
    DL_Timer_startCounter(PWM_0_INST);

    secondsSinceStart = 0;
    return 0;
}

uint32_t initPulseConfig(PulseConfig *ptrPulseConfig, uint8_t maxBrightness){
    // Active LED's brightness will "pulse"
    // max value of this pulse is the global Brightness of the device (configurable)
    // min value of this pulse is set to a default (my default of 32)

    // note that this ties the pulse rate to the max and min brightnesses. a problem to solve another day. 

    ptrPulseConfig->pulseCCMPVal = maxBrightness;
    ptrPulseConfig->pulseIncrementDirection = 1;

    ptrPulseConfig->pulseIncrementSize = 1;
    ptrPulseConfig->pulseLowerBound = 32;
    ptrPulseConfig->pulseUpperBound = maxBrightness;
    return 0;
}

void updateLedStatuses(Pomodoro *ptrPomoTimer, GPIOWrapper* ledArray, int32_t size){
    // Update physical LEDs to reflect masks in pomodoro timer struct. 
    // call this whenever you change which LEDs are active in the struct 

    // turn on all LEDs in the litLedMask, everything else off
    for(int i = 0; i < NUMBER_OF_LEDS; i++){

        if(((ptrPomoTimer->litLEDMask)>>i)%2){ // should spit out the mask one at a time, spitting out index[0] first
            // if that LED is in the mask, turn it on
            // this uses the helpful ledArray struct to let us iterate over LEDs w/o hardcoding..
            DL_GPIO_setPins(ledArray[i].gpioRegs, ledArray[i].pinNumber);
        }
        else{
            DL_GPIO_clearPins(ledArray[i].gpioRegs, ledArray[i].pinNumber);
        }
        
    }

    //zero out pulseCCMPVal to make transition smoother
    PulseData.pulseCCMPVal = 0;
    // then change the active LED to be whatever active value
}

void incrementPomoPeriod(Pomodoro *ptrPomoTimer){
    if(ptrPomoTimer->activePeriod > 0){
        // if there are still more periods left
        ptrPomoTimer->activePeriod -= 1;
    }
    else{
        // no more periods left
        // kill the last light
        DL_GPIO_clearPins(ledArray[PomoTimer.activePeriod].gpioRegs, ledArray[PomoTimer.activePeriod].pinNumber);
  
        //stop timers here, wait for reset
        DL_Timer_stopCounter(SECOND_TICKER_INST);
        DL_Timer_stopCounter(PULSE_TIMER_INST);
        DL_Timer_stopCounter(PWM_0_INST);

    }

    // update litLEDMask to turn off the active (pulsing)
    ptrPomoTimer->litLEDMask = ptrPomoTimer->litLEDMask ^ ptrPomoTimer->activePeriodLEDMask;
    // update active LED Mask to reflect the active period, making this the pulsing one
    ptrPomoTimer->activePeriodLEDMask = 1 << ptrPomoTimer->activePeriod;

    // change our LEDs to be on / off
    updateLedStatuses(ptrPomoTimer, ledArray, NUMBER_OF_LEDS);
}

uint32_t initLedArray(GPIOWrapper *ledArray, int32_t size){
    // you'll have to manually declare your LEDs here

    /*
    //LED defines for launchpad
    // LED 1
    ledArray[0].gpioRegs = GPIOB;
    ledArray[0].pinNumber = GPIO_GRP_0_LED_1_PIN;
    // LED 2
    ledArray[1].gpioRegs = GPIOB;
    ledArray[1].pinNumber = GPIO_GRP_0_LED_2_PIN;
    // LED 3
    ledArray[2].gpioRegs = GPIOB;
    ledArray[2].pinNumber = GPIO_GRP_0_LED_3_PIN;
    // LED 4
    */

    
    ledArray[0].gpioRegs = GPIOA;
    ledArray[0].pinNumber = GPIO_GRP_0_REAL_LED_0_PIN;

    ledArray[1].gpioRegs = GPIOA;
    ledArray[1].pinNumber = GPIO_GRP_0_REAL_LED_1_PIN;

    ledArray[2].gpioRegs = GPIOA;
    ledArray[2].pinNumber = GPIO_GRP_0_REAL_LED_2_PIN;

    ledArray[3].gpioRegs = GPIOA;
    ledArray[3].pinNumber = GPIO_GRP_0_REAL_LED_3_PIN;
    
    ledArray[4].gpioRegs = GPIOB;
    ledArray[4].pinNumber = GPIO_GRP_0_REAL_LED_4_PIN;
    
    
    // LED 5

    return 0;
}

void SECOND_TICKER_INST_IRQHandler(){
    // know I have to do this to clear interrupt and allow it to trigger again (prior knowledge)
    uint32_t secondsPerPeriod = SECONDS_PER_PERIOD;
    DL_Timer_getPendingInterrupt(SECOND_TICKER_INST);
    secondsSinceStart += 1;
    if (secondsSinceStart % secondsPerPeriod == 0)
    {
        incrementPomoPeriod(&PomoTimer);
        periodIncrement += 1;
    }
}

void PWM_0_INST_IRQHandler(){
    // tried to do this in HW, but it wasn't flexible enough to change which LED to trigger. 
    // UPDATE: now I'll have to pipe in which LED.

    switch (DL_TimerG_getPendingInterrupt(PWM_0_INST)) {
        case DL_TIMER_IIDX_CC0_UP:
            // turn LED off 
            DL_GPIO_clearPins(ledArray[PomoTimer.activePeriod].gpioRegs, ledArray[PomoTimer.activePeriod].pinNumber);
            break;
        case DL_TIMER_IIDX_ZERO:
            // turn LED on
            DL_GPIO_setPins(ledArray[PomoTimer.activePeriod].gpioRegs, ledArray[PomoTimer.activePeriod].pinNumber);
            break;
        default:
            break;
    }
}

void PULSE_TIMER_INST_IRQHandler(){
    // need to ensure it's the right interrupt??
    DL_Timer_getPendingInterrupt(PULSE_TIMER_INST);
    
    if(PulseData.pulseCCMPVal >= PulseData.pulseUpperBound){
        PulseData.pulseIncrementDirection = -1;
    }
    else if (PulseData.pulseCCMPVal <= PulseData.pulseLowerBound){
        PulseData.pulseIncrementDirection = 1;
    }
    PulseData.pulseCCMPVal += PulseData.pulseIncrementSize * PulseData.pulseIncrementDirection;
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, ledLookupTable[PulseData.pulseCCMPVal], DL_TIMER_CC_0_INDEX);
}

void GROUP1_IRQHandler(void){ 
    // reset pomodoro
    //__BKPT(0);
    switch(DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)){
        case GPIO_GRP_0_INT_IIDX:
            /*
            for(int i = 0; i < NUMBER_OF_LEDS; i++){
                DL_GPIO_setPins(ledArray[i].gpioRegs, ledArray[i].pinNumber);
            }*/
            count +=1;
            initPomoTimer(&PomoTimer);
            initPulseConfig(&PulseData, mb);
            break;
    }
}
