
//---CONFIGURATION VARIABLES---


//---POMODORO TIMER BEHAVIOR---
//standard Pomodoro timer is 25 min. of work. so with 5x LEDs, this is 5x 60 second periods
#define NUMBER_OF_LEDS 3
//change to 60 in prod
#define SECONDS_PER_PERIOD 2;

//---LED PULSE BEHAVIOR---
#define MAXBRIGHTNESS 160

typedef struct gpioWrapper{
    // GPIO value is a macro, which looks like a big ol hex value
    GPIO_Regs* gpioRegs;
    uint32_t pinNumber;
} GPIOWrapper;

typedef struct pomodoro{
    //could do an array but why? just pass bits around. 

    // normally static values
    uint32_t litLEDMask;
    uint32_t existingLEDMask;
    uint32_t activePeriod;
    uint32_t activePeriodLEDMask;
    //uint32_t secondsSinceStart;
} Pomodoro;

typedef struct pulseConfig{
    //this struct stores information for discrete pulse brightness steps 

    // real-time values
    uint32_t pulseCCMPVal;
    int32_t pulseIncrementDirection;        

    // normally static values 
    uint32_t pulseLowerBound;
    uint32_t pulseUpperBound;
    uint32_t pulseIncrementSize;
} PulseConfig;

uint32_t initPomoTimer(Pomodoro *ptrPomoTimer);
uint32_t initPulseConfig(PulseConfig *ptrPulseConfig, uint8_t maxBrightness);
uint32_t initLedArray(GPIOWrapper *ledArray, int32_t size);
void incrementPomoPeriod(Pomodoro *ptrPomoTimer);
void updateLEDStatuses(Pomodoro *ptrPomoTimer, GPIOWrapper* ledArray, int32_t size);