/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 6  Exercise 2


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/n1buA6OI4I4

 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include "timerISR.h"

int phases[8] = {0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001};

unsigned char direction = 0;        
unsigned char buffer[4];            
unsigned char bufIdx = 0;           
unsigned char stepCount = 0;        
unsigned char locked = 1;           
signed char motorDir = 1;           
unsigned char lockTrigger = 0;      
unsigned char motorBusy = 0;        
unsigned char errorFlag = 0;        
unsigned long errorTime = 0;        
unsigned long blinkTimer = 0;       

void ADC_init() {
    ADMUX  = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

unsigned int ADC_read(uint8_t ch) {
    ch &= 0x07;
    ADMUX = (ADMUX & 0xF8) | ch;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

#define NUM_TASKS 5
const unsigned long TASK0_PERIOD = 50;   
const unsigned long TASK1_PERIOD = 100;  
const unsigned long TASK2_PERIOD = 50;   
const unsigned long TASK3_PERIOD = 50;   
const unsigned long TASK4_PERIOD = 10;   
const unsigned long GCD_PERIOD   = 10;

#define PHASE_INC 2

typedef struct _task {
    signed   char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;
task tasks[NUM_TASKS];

#define VRX_CH   2       
#define VRY_CH   3       
#define SW_BIT   (1<<PC4)
#define SW_PIN   PINC
#define LED_MASK 0x03    

const uint8_t segMap[5] = {
    0b00000001, 
    0b01111100, 
    0b00001001, 
    0b01111001, 
    0b00011100  
};

enum T0_States { T0_READ };
enum T1_States { T1_DISP };
enum T2_States { T2_WAIT_CENTER, T2_WAIT_RELEASE, T2_PROCESS };
enum T3_States { T3_IDLE, T3_BLINK };
enum T4_States { T4_IDLE, T4_RUN };

const unsigned char passcode[4] = {1,3,4,2}; 

int TickFct_Task0(int state) {
    unsigned int vx = ADC_read(VRX_CH);
    unsigned int vy = ADC_read(VRY_CH);
    if (vx > 800)      direction = 1; 
    else if (vx < 200) direction = 3;  
    else if (vy > 800) direction = 2;  
    else if (vy < 200) direction = 4;  
    else               direction = 0;  
    return T0_READ;
}

int TickFct_Task1(int state) {
    uint8_t seg = segMap[direction];
    PORTD = (PORTD & 0x03) | (seg & 0xFC); 
    PORTB = (PORTB & 0xFC) | (seg & 0x03); 
    return T1_DISP;
}

int TickFct_Task2(int state) {
    switch(state) {
        case T2_WAIT_CENTER:
            if (direction != 0) {
                buffer[bufIdx] = direction;
                state = T2_WAIT_RELEASE;
            }
            break;
        case T2_WAIT_RELEASE:
            if (direction == 0) {
                bufIdx++;
                stepCount = bufIdx;
                if (bufIdx >= 4) state = T2_PROCESS;
                else             state = T2_WAIT_CENTER;
            }
            break;
        case T2_PROCESS: {
            unsigned char ok = 1;
            for (uint8_t i = 0; i < 4; ++i) {
                if (buffer[i] != passcode[i]) ok = 0;
            }
            if (ok) {
                locked = !locked;
                motorDir = locked ? -1 : 1;
                lockTrigger = 1;
            } else {
                errorFlag = 1;
                errorTime = 0;
                blinkTimer = 0;
            }
            bufIdx = 0;
            stepCount = 0;
            state = T2_WAIT_CENTER;
            break;
        }
    }
    return state;
}

int TickFct_Task3(int state) {
    static unsigned char blinkState = 0;
    if (motorBusy) {
        PORTC = LED_MASK;
    } else if (errorFlag) {
        errorTime += TASK3_PERIOD;
        blinkTimer += TASK3_PERIOD;
        if (blinkTimer >= 500) {
            blinkTimer -= 500;
            blinkState ^= 1;
        }
        PORTC = blinkState ? LED_MASK : 0;
        if (errorTime >= 4000) {
            errorFlag = 0;
            PORTC = 0;
        }
    } else {
        if (stepCount >= 1 && stepCount <= 3) PORTC = stepCount;
        else PORTC = 0;
    }
    return T3_IDLE;
}

int TickFct_Task4(int state) {
    static int phase = 0;
    static int steps = 0;
    switch(state) {
        case T4_IDLE:
            if (lockTrigger) {
                lockTrigger = 0;
                motorBusy = 1;
                steps = 1024 / PHASE_INC;
                state = T4_RUN;
            }
            break;
        case T4_RUN:
            PORTB = (PORTB & 0xC3) | (phases[phase] << 2);
            phase = (phase + motorDir*PHASE_INC + 8) % 8;
            if (--steps <= 0) {
                motorBusy = 0;
                state = T4_IDLE;
            }
            break;
    }
    return state;
}

void TimerISR() {
    for (unsigned char i = 0; i < NUM_TASKS; ++i) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

int main(void) {
    DDRC  = LED_MASK;
    PORTC = SW_BIT;
    DDRB  = 0x3F;
    PORTB = 0x00;
    DDRD  = 0xFC;
    PORTD = 0x00;

    ADC_init();
    tasks[0] = (task){T0_READ, TASK0_PERIOD, TASK0_PERIOD, &TickFct_Task0};
    tasks[1] = (task){T1_DISP, TASK1_PERIOD, TASK1_PERIOD, &TickFct_Task1};
    tasks[2] = (task){T2_WAIT_CENTER, TASK2_PERIOD, TASK2_PERIOD, &TickFct_Task2};
    tasks[3] = (task){T3_IDLE, TASK3_PERIOD, TASK3_PERIOD, &TickFct_Task3};
    tasks[4] = (task){T4_IDLE, TASK4_PERIOD, TASK4_PERIOD, &TickFct_Task4};

    TimerSet(GCD_PERIOD);
    TimerOn();
    sei();
    while (1) {}
    return 0;
}