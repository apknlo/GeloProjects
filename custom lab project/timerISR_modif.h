#ifndef TIMER_H
#define TIMER_H

#include <avr/interrupt.h>
#include <avr/io.h>

volatile unsigned char TimerFlag = 0; 
unsigned long _avr_timer_M = 1; 
unsigned long _avr_timer_cntcurr = 0; 

void TimerISR(void); 
void TimerSet(unsigned long M) {
    _avr_timer_M = M;
    _avr_timer_cntcurr = _avr_timer_M;
}

void TimerOn() {
    TCCR1A = 0x00; 
    TCCR1B = 0x00;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A = 249;
    TIMSK1 = (1 << OCIE1A);
    TCNT1 = 0;
    _avr_timer_cntcurr = _avr_timer_M;
    sei(); 
}

void TimerOff() {
    TCCR1B = 0x00; 
}

ISR(TIMER1_COMPA_vect) {
    _avr_timer_cntcurr--; 
    if (_avr_timer_cntcurr == 0) { 
        TimerISR();                
        _avr_timer_cntcurr = _avr_timer_M; 
    }
}

#endif 