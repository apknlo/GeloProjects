/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 5  Exercise 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/IJElg76IJkI

 */

#include "timerISR.h"
#include "helper.h"
#include "periph.h"


volatile unsigned int distanceCm = 0;
volatile unsigned char unitFlag  = 0;  


#define NUM_TASKS 3

typedef struct _task {
    signed   char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

task tasks[NUM_TASKS];

const unsigned long TASK1_PERIOD = 1000;  
const unsigned long TASK2_PERIOD =    1;  
const unsigned long TASK3_PERIOD =  200;  

const unsigned long GCD_PERIOD =
    findGCD(findGCD(TASK1_PERIOD, TASK2_PERIOD), TASK3_PERIOD);

enum SonarStates { SONAR_READ };
int  Tick_Sonar(int);

enum DispStates  { DISP_D1, DISP_D2, DISP_D3, DISP_D4 };
int  Tick_Display(int);

enum BtnStates   { BTN_WAIT_PRESS, BTN_WAIT_RELEASE };
int  Tick_Button(int);

void TimerISR() {
    for (unsigned i = 0; i < NUM_TASKS; ++i) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state       = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

int main(void) {

    DDRC  = (1<<PC2);          
    PORTC = (1<<PC0);          

    DDRB  = (1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5);
    PORTB = 0x00;               

    DDRD  = (1<<PD2)|(1<<PD3)|(1<<PD4)
          | (1<<PD5)|(1<<PD6)|(1<<PD7);
    PORTD = 0x00;

    ADC_init();
    sonar_init();

    tasks[0] = (task){ .state=SONAR_READ,     .period=TASK1_PERIOD, .elapsedTime=0, .TickFct=Tick_Sonar };
    tasks[1] = (task){ .state=DISP_D1,        .period=TASK2_PERIOD, .elapsedTime=0, .TickFct=Tick_Display };
    tasks[2] = (task){ .state=BTN_WAIT_PRESS, .period=TASK3_PERIOD, .elapsedTime=0, .TickFct=Tick_Button };

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}
    return 0;
}

int Tick_Sonar(int state) {
    double d = sonar_read();
    distanceCm = (unsigned int)(d + 0.5);
    return SONAR_READ;
}

int Tick_Display(int state) {
    unsigned int val = distanceCm;
    if (unitFlag) {
        val = (unsigned int)(val * 0.393701 + 0.5);
    }
    unsigned char digs[4] = {
        (val / 1000) % 10,
        (val /  100) % 10,
        (val /   10) % 10,
         val         % 10
    };

    PORTB |= (1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5);
    PORTD &= ~((1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7));
    PORTB &= ~(1<<PB1);

    outNum(digs[state]);
    switch (state) {
      case DISP_D1: PORTB &= ~(1<<PB2); break;
      case DISP_D2: PORTB &= ~(1<<PB3); break;
      case DISP_D3: PORTB &= ~(1<<PB4); break;
      case DISP_D4: PORTB &= ~(1<<PB5); break;
    }
    return (state + 1) % 4;
}

int Tick_Button(int state) {
    unsigned char pressed = !(PINC & (1<<PC0));
    switch (state) {
      case BTN_WAIT_PRESS:
        if (pressed) {
          unitFlag = !unitFlag;
          state = BTN_WAIT_RELEASE;
        }
        break;
      case BTN_WAIT_RELEASE:
        if (!pressed) {
          state = BTN_WAIT_PRESS;
        }
        break;
    }
    return state;
}
