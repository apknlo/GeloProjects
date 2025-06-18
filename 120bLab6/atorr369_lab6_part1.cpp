/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 6  Exercise 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/gtfoyk7wVpk

 */

#include <avr/interrupt.h>
#include <avr/io.h>

#include "timerISR.h"

int phases[8] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001}; // stepper phases

unsigned char direction = 0;  
unsigned char count2 = 0;       

void ADC_init() {
    ADMUX  = (1 << REFS0);                      
    ADCSRA = (1 << ADEN) | (1 << ADPS2)
           | (1 << ADPS1) | (1 << ADPS0);       
}

unsigned int ADC_read(uint8_t ch) {
    ch &= 0x07;                                 
    ADMUX = (ADMUX & 0xF8) | ch;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));               
    return ADC;
}

#define NUM_TASKS 3
const unsigned long TASK0_PERIOD = 50;
const unsigned long TASK1_PERIOD = 100;
const unsigned long TASK2_PERIOD = 50;
const unsigned long GCD_PERIOD   = 50;

typedef struct _task {
    signed   char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

task tasks[NUM_TASKS];

#define VRX_CH 2       
#define VRY_CH 3       
#define SW_BIT  (1<<PC4)
#define SW_PIN  PINC

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
enum T2_States { T2_WAIT, T2_DEB };

int TickFct_Task0(int state) {
    unsigned int vx = ADC_read(VRX_CH);
    unsigned int vy = ADC_read(VRY_CH);
    unsigned char pressed = !(SW_PIN & SW_BIT);

    if (!pressed) {
        if (vx > 800)      direction = 1; 
        else if (vx < 200) direction = 3; 
        else if (vy > 800) direction = 2; 
        else if (vy < 200) direction = 4; 
        else               direction = 0;
    }
    return T0_READ;
}

int TickFct_Task1(int state) {
    uint8_t seg = segMap[direction];
    PORTD = (PORTD & 0x03) | (seg & 0xFC);
    PORTB = (PORTB & 0xFC) | (seg & 0x03);
    return T1_DISP;
}

int TickFct_Task2(int state) {
    unsigned char pressed = !(SW_PIN & SW_BIT);
    switch(state) {
        case T2_WAIT:
            if (pressed) {
                count2 = (count2 + 1) & 0x03;
                PORTC = (PORTC & ~LED_MASK) | count2;
                state = T2_DEB;
            }
            break;
        case T2_DEB:
            if (!pressed) state = T2_WAIT;
            break;
    }
    return state;
}

void TimerISR() {
    for (uint8_t i = 0; i < NUM_TASKS; ++i) {
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
    tasks[0] = (task){.state=T0_READ,.period=TASK0_PERIOD,.elapsedTime=0,.TickFct=&TickFct_Task0};
    tasks[1] = (task){.state=T1_DISP,.period=TASK1_PERIOD,.elapsedTime=0,.TickFct=&TickFct_Task1};
    tasks[2] = (task){.state=T2_WAIT,.period=TASK2_PERIOD,.elapsedTime=0,.TickFct=&TickFct_Task2};

    TimerSet(GCD_PERIOD);
    TimerOn();
    sei();

    while (1) {}
    return 0;
}
