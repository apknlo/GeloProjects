/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 7  Exercise 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/cCfJluYLzRs

 */

#include "timerISR.h"
#include "helper.h"
#include "periph.h"

#define NUM_TASKS 2

typedef struct _task{
    signed char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

const unsigned long GCD_PERIOD  = 50;
const unsigned long IND_PERIOD  = 400;
const unsigned long HORN_PERIOD = 50;

task tasks[NUM_TASKS];

void TimerISR(){
    for(unsigned int i=0;i<NUM_TASKS;i++){
        if(tasks[i].elapsedTime == tasks[i].period){
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

#define LEDL1 PD5  
#define LEDL2 PD7  
#define LEDL3 PB0  
#define LEDR1 PD4  
#define LEDR2 PD3  
#define LEDR3 PD2  
#define BTN_LEFT()   (!(PINC & (1<<PC4)))  
#define BTN_RIGHT()  (!(PINC & (1<<PC3)))  
#define JOY_BTN()    (!(PINC & (1<<PC2)))

enum IndStates{ IND_IDLE, IND_LEFT, IND_RIGHT };

static const uint8_t patL_D[4] = { 0,
    (1<<LEDL2),
    (1<<LEDL2)|(1<<LEDL1),
    0
};
static const uint8_t patL_B[4] = { (1<<LEDL3), (1<<LEDL3), (1<<LEDL3), 0 };

static const uint8_t patR_D[4] = { (1<<LEDR1),
    (1<<LEDR1)|(1<<LEDR2),
    (1<<LEDR1)|(1<<LEDR2)|(1<<LEDR3),
    0
};

int TickF_Indicator(int state){
    static uint8_t idx=0, prev=0;
    prev = state;
    
    if(BTN_LEFT() && !BTN_RIGHT())      state = IND_LEFT;
    else if(BTN_RIGHT() && !BTN_LEFT()) state = IND_RIGHT;
    else                                state = IND_IDLE;
    if(state != prev) idx = 0;

    PORTD &= ~((1<<LEDL1)|(1<<LEDL2)|(1<<LEDR1)|(1<<LEDR2)|(1<<LEDR3));
    PORTB &= ~(1<<LEDL3);

    if(state == IND_RIGHT){
        PORTD |= patL_D[idx];
        PORTB |= patL_B[idx];
    } else if(state == IND_LEFT) {
        PORTD |= patR_D[idx];
    }

    if(state != IND_IDLE) idx = (idx + 1) & 0x03;
    return state;
}

int TickF_Horn(int state){
    OCR0A = JOY_BTN()?128:255;
    return state;
}

void initBuzzerPWM(void){
    DDRD |= (1<<PD6);
    TCCR0A = (1<<COM0A1)|(1<<WGM01)|(1<<WGM00);
    TCCR0B = (TCCR0B & 0xF8)|0x03;
    OCR0A = 255;
}

void initServoPWM(void){
    DDRB |= (1<<PB1);
    TCCR1A = (1<<WGM11)|(1<<COM1A1);
    TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11);
    ICR1 = 39999;
}

int main(void){
    DDRD |= (1<<LEDL1)|(1<<LEDL2)|(1<<LEDR1)|(1<<LEDR2)|(1<<LEDR3);
    DDRB |= (1<<LEDL3);
    DDRC &= ~((1<<PC4)|(1<<PC3)|(1<<PC2));
    PORTC |=  (1<<PC4)|(1<<PC3)|(1<<PC2);

    ADC_init();
    initBuzzerPWM();
    initServoPWM();

    tasks[0].period = IND_PERIOD;
    tasks[0].state = IND_IDLE;
    tasks[0].elapsedTime = IND_PERIOD;
    tasks[0].TickFct = &TickF_Indicator;

    tasks[1].period = HORN_PERIOD;
    tasks[1].state = 0;
    tasks[1].elapsedTime = HORN_PERIOD;
    tasks[1].TickFct = &TickF_Horn;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while(1){}
    return 0;
}

