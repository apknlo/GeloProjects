/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 4  Exercise 3


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *       

 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "timerISR.h"

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
    return b ? (x | (1<<k)) : (x & ~(1<<k));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
    return (x & (1<<k)) != 0;
}

const unsigned char nums[17] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
    0, 0,
    0b0111001, // 12 = 'd'
    0, 0, 0,
    0b1000000  // 16 = '-'
};

#define DARK_THRESH 512  // adjust after you measure your divider

// drive D4 (rightmost)  
void outNum(unsigned char idx) {
    PORTB = nums[idx] & 0x3F;
    unsigned char pd = PORTD & ((1<<PD2)|(1<<PD3)|(1<<PD4));
    if (nums[idx] & (1<<6)) pd |=  (1<<PD7); else pd &= ~(1<<PD7);
    pd &= ~(1<<PD5);  // D4 on
    pd |=  (1<<PD6);  // D1 off
    PORTD = pd;
}
// drive D1 (leftmost)
void outNumD1(unsigned char idx) {
    PORTB = nums[idx] & 0x3F;
    unsigned char pd = PORTD & ((1<<PD2)|(1<<PD3)|(1<<PD4));
    if (nums[idx] & (1<<6)) pd |=  (1<<PD7); else pd &= ~(1<<PD7);
    pd |=  (1<<PD5);  // D4 off
    pd &= ~(1<<PD6);  // D1 on
    PORTD = pd;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min)*(out_max - out_min)/(in_max - in_min) + out_min;
}

void ADC_init(void) {
    ADMUX  = (1<<REFS0);
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
    ADCSRB = 0x00;
    DIDR0  = (1<<ADC0D)|(1<<ADC1D);
}
unsigned int ADC_read(unsigned char chnl){
    ADMUX = (ADMUX & 0xF8) | (chnl & 0x07);
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC));
    uint8_t lo = ADCL, hi = ADCH;
    return (hi<<8)|lo;
}

enum states { DISPLAY, SNOOZE_RELEASE, SNOOZE_COUNTDOWN, DANGER };
static enum states state = DISPLAY;

void Tick(void) {
    static unsigned char snooze_ticks = 0, blink = 0;
    static unsigned char value = 0;

    // inputs
    unsigned char btn      = GetBit(PINC, PC2);   // 1=up,0=pressed
    unsigned int rawPot    = ADC_read(1);
    unsigned int rawPhoto  = ADC_read(0);
    unsigned char isDark   = (rawPhoto < DARK_THRESH);

    // always update mapped pot except during countdown
    if (state != SNOOZE_COUNTDOWN)
        value = (unsigned char)map(rawPot, 0, 1023, 0, 9);

    // transitions
    switch(state) {
      case DISPLAY:
        if (!btn)                    state = SNOOZE_RELEASE;
        else if (isDark && value>3)  state = DANGER;
        break;
      case SNOOZE_RELEASE:
        if (btn) { snooze_ticks = 6; state = SNOOZE_COUNTDOWN; }
        break;
      case SNOOZE_COUNTDOWN:
        if (snooze_ticks == 0)       state = DISPLAY;
        break;
      case DANGER:
        if (!btn)                    state = SNOOZE_RELEASE;
        else if (!isDark || value<=3)state = DISPLAY;
        break;
    }

    // actions
    switch(state) {
      case DISPLAY:
      case SNOOZE_RELEASE:
        if (isDark && value <= 3) {
          // dark + small → D1 + blue
          outNumD1(value);
          PORTD = (PORTD & ~((1<<PD3)|(1<<PD4))) | (1<<PD2);
        } else {
          // bright or dark+large fallback → ex2 on D4
          outNum(value);
          {
            unsigned char pd = PORTD & ~((1<<PD2)|(1<<PD3)|(1<<PD4));
            if      (value <= 3) pd |=  (1<<PD2);
            else if (value < 8)  pd |= ((1<<PD3)|(1<<PD4));
            else                  pd |=  (1<<PD4);
            PORTD = pd;
          }
        }
        break;

      case SNOOZE_COUNTDOWN:
        outNum(16);              // “–” on D4
        PORTD &= ~((1<<PD2)|(1<<PD3)|(1<<PD4));  // LED off
        if (snooze_ticks) --snooze_ticks;
        break;

      case DANGER:
        blink ^= 1;
        if (blink)   outNumD1(12);  // blink 'd'
        else {
          PORTB = 0;                // clear segments
          PORTD |= (1<<PD5)|(1<<PD6)|(1<<PD7); // all digits off
        }
        // LED = red
        PORTD = (PORTD & ~((1<<PD2)|(1<<PD3))) | (1<<PD4);
        break;
    }
}

int main(void) {
    DDRB  = 0x3F;  PORTB = 0x00;       // PB0–PB5 segments
    DDRC  = 0x00;  PORTC = (1<<PC2);   // PC0/PC1 ADC, PC2 button pull-up
    DDRD  = 0b11111100;  PORTD = 0x00; // PD2–4 LEDs, PD5=D4, PD6=D1, PD7=g

    ADC_init();
    TimerSet(500);
    TimerOn();

    while (1) {
        Tick();
        while (!TimerFlag) {}
        TimerFlag = 0;
    }
    return 0;
}












