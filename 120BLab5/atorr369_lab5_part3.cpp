/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 5  Exercise 3


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/h0JLSFuychU

 */

#include "timerISR.h"
#include "helper.h"
#include "periph.h"

volatile unsigned int distanceCm = 0;
volatile unsigned char unitFlag  = 0;    
volatile unsigned int threshold_close;
volatile unsigned int threshold_far;
volatile unsigned char blinkActive = 0;
volatile unsigned char blinkTicks  = 0;


#define BTN_LEFT    PC0
#define BTN_RIGHT   PC1
#define SONAR_TRIG  PC2
#define SONAR_ECHO  PB0

#define SEG_A       PD7
#define SEG_B       PD6
#define SEG_C       PD5
#define SEG_D       PD4
#define SEG_E       PD3
#define SEG_F       PD2
#define SEG_G       PB1

#define DIG1        PB2
#define DIG2        PB3
#define DIG3        PB4
#define DIG4        PB5

#define RED_PIN     PC3
#define GREEN_PIN   PC4
#define BLUE_PIN    PC5

#define NUM_TASKS 7

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
const unsigned long TASK4_PERIOD =    1;  
const unsigned long TASK5_PERIOD =    1;  
const unsigned long TASK6_PERIOD =  200;  
const unsigned long TASK7_PERIOD =  250;  

const unsigned long GCD_PERIOD =
    findGCD(
      findGCD(findGCD(TASK1_PERIOD, TASK2_PERIOD), TASK3_PERIOD),
      findGCD(findGCD(TASK4_PERIOD, TASK5_PERIOD), findGCD(TASK6_PERIOD, TASK7_PERIOD))
    );

enum SonarStates { SONAR_READ };
int  Tick_Sonar(int);

enum DispStates  { DISP_D1, DISP_D2, DISP_D3, DISP_D4 };
int  Tick_Display(int);

enum BtnLStates  { LBTN_WAIT_PRESS, LBTN_WAIT_RELEASE };
int  Tick_LeftBtn(int);

enum RedStates   { RED_PHASE };
int  Tick_RedPWM(int);

enum GreenStates { GREEN_PHASE };
int  Tick_GreenPWM(int);

enum BtnRStates  { RBTN_WAIT_PRESS, RBTN_WAIT_RELEASE };
int  Tick_RightBtn(int);

enum BlueStates  { BLUE_OFF, BLUE_ON };
int  Tick_Blue(int);

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

    DDRC  = (1<<SONAR_TRIG)|(1<<RED_PIN)|(1<<GREEN_PIN)|(1<<BLUE_PIN);
    PORTC = (1<<BTN_LEFT)|(1<<BTN_RIGHT);

    DDRB  = (1<<SEG_G)|(1<<DIG1)|(1<<DIG2)|(1<<DIG3)|(1<<DIG4);
    PORTB = (1<<SONAR_ECHO);

    DDRD  = (1<<SEG_A)|(1<<SEG_B)|(1<<SEG_C)
          | (1<<SEG_D)|(1<<SEG_E)|(1<<SEG_F);
    PORTD = 0x00;

    ADC_init();
    sonar_init();

    threshold_close = (10 * 4) / 5;
    threshold_far   = (10 * 6) / 5;

    tasks[0] = (task){ .state=SONAR_READ,      .period=TASK1_PERIOD, .elapsedTime=0, .TickFct=Tick_Sonar };
    tasks[1] = (task){ .state=DISP_D1,         .period=TASK2_PERIOD, .elapsedTime=0, .TickFct=Tick_Display };
    tasks[2] = (task){ .state=LBTN_WAIT_PRESS, .period=TASK3_PERIOD, .elapsedTime=0, .TickFct=Tick_LeftBtn };
    tasks[3] = (task){ .state=0,               .period=TASK4_PERIOD, .elapsedTime=0, .TickFct=Tick_RedPWM };
    tasks[4] = (task){ .state=0,               .period=TASK5_PERIOD, .elapsedTime=0, .TickFct=Tick_GreenPWM };
    tasks[5] = (task){ .state=RBTN_WAIT_PRESS, .period=TASK6_PERIOD, .elapsedTime=0, .TickFct=Tick_RightBtn };
    tasks[6] = (task){ .state=BLUE_OFF,        .period=TASK7_PERIOD, .elapsedTime=0, .TickFct=Tick_Blue };

    TimerSet(GCD_PERIOD);
    TimerOn();
    while (1) { }
    return 0;
}

int Tick_Sonar(int state) {
    distanceCm = (unsigned int)(sonar_read() + 0.5);
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

    PORTB |= (1<<DIG1)|(1<<DIG2)|(1<<DIG3)|(1<<DIG4);
    PORTD &= ~((1<<SEG_A)|(1<<SEG_B)|(1<<SEG_C)
             | (1<<SEG_D)|(1<<SEG_E)|(1<<SEG_F));
    PORTB &= ~(1<<SEG_G);

    outNum(digs[state]);
    switch (state) {
      case DISP_D1: PORTB &= ~(1<<DIG1); break;
      case DISP_D2: PORTB &= ~(1<<DIG2); break;
      case DISP_D3: PORTB &= ~(1<<DIG3); break;
      case DISP_D4: PORTB &= ~(1<<DIG4); break;
    }
    return (state + 1) % 4;
}

int Tick_LeftBtn(int state) {
    unsigned char pressed = !(PINC & (1<<BTN_LEFT));
    switch (state) {
      case LBTN_WAIT_PRESS:
        if (pressed) {
          unitFlag = !unitFlag;
          state = LBTN_WAIT_RELEASE;
        }
        break;
      case LBTN_WAIT_RELEASE:
        if (!pressed) {
          state = LBTN_WAIT_PRESS;
        }
        break;
    }
    return state;
}

int Tick_RedPWM(int phase) {
    if (blinkActive) {
        PORTC &= ~(1<<RED_PIN);
        return (phase + 1) % 10;
    }
    unsigned duty = (distanceCm <  threshold_close) ? 10 :
                    (distanceCm <= threshold_far)  ? 9  : 0;
    if (phase < duty) PORTC |=  (1<<RED_PIN);
    else              PORTC &= ~(1<<RED_PIN);
    return (phase + 1) % 10;
}

int Tick_GreenPWM(int phase) {
    if (blinkActive) {
        PORTC &= ~(1<<GREEN_PIN);
        return (phase + 1) % 10;
    }
    unsigned duty = (distanceCm <  threshold_close) ? 0  :
                    (distanceCm <= threshold_far)  ? 3  : 10;
    if (phase < duty) PORTC |=  (1<<GREEN_PIN);
    else              PORTC &= ~(1<<GREEN_PIN);
    return (phase + 1) % 10;
}

int Tick_RightBtn(int state) {
    unsigned char pressed = !(PINC & (1<<BTN_RIGHT));
    switch (state) {
      case RBTN_WAIT_PRESS:
        if (pressed) {
          blinkActive = 1;
          blinkTicks  = 0;
          unsigned int d = distanceCm;
          threshold_close = (d * 4) / 5;
          threshold_far   = (d * 6) / 5;
          state = RBTN_WAIT_RELEASE;
        }
        break;
      case RBTN_WAIT_RELEASE:
        if (!pressed) {
          state = RBTN_WAIT_PRESS;
        }
        break;
    }
    return state;
}

int Tick_Blue(int state) {
    if (!blinkActive) {
        PORTC &= ~(1<<BLUE_PIN);
        return BLUE_OFF;
    }
    if (state == BLUE_OFF) {
        PORTC |=  (1<<BLUE_PIN);
        state = BLUE_ON;
    } else {
        PORTC &= ~(1<<BLUE_PIN);
        state = BLUE_OFF;
    }
    if (++blinkTicks >= 12) {
        blinkActive = 0;
        blinkTicks  = 0;
        PORTC &= ~(1<<BLUE_PIN);
        state = BLUE_OFF;
    }
    return state;
}
