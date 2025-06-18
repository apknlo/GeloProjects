#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "timerISR.h"

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
   return (b ? (x | (1 << k)) : (x & ~(1 << k)));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
   return ((x & (1 << k)) != 0);
}

const unsigned char nums[16] = {
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
  0b1110111, // A
  0b1111100, // b
  0b0111001, // C
  0b1011110, // d
  0b1111001, // E
  0b1110001  // F
};

void displayNum(unsigned char idx) {
    PORTB = nums[idx] & 0x1F;
    uint8_t pd = PORTD & (1<<PD2);
    pd |= ( (nums[idx] & 0x60) << 1 );
    PORTD = pd;
}

void ADC_init(void) {
    ADMUX  = (1<<REFS0);                                 
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);  
    ADCSRB = (1<<ACME);                                  
}
unsigned int ADC_read(unsigned char chnl){
    ADMUX  = (ADMUX & 0xF0) | (chnl & 0x07);  
    ADCSRA |= (1<<ADSC);                      
    while (ADCSRA & (1<<ADSC));               
    uint8_t low  = ADCL;
    uint8_t high = ADCH;
    return (high<<8) | low;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min)
       / (in_max - in_min) + out_min;
}

enum states { AM_DISPLAY, AM_WAIT, FM_DISPLAY, FM_WAIT };
static enum states state = AM_DISPLAY;

void Tick(void) {
    static unsigned char value;
    unsigned char btn = GetBit(PINC, PC1);

    switch(state) {
      case AM_DISPLAY:
        if (!btn) state = AM_WAIT;
        break;
      case AM_WAIT:
        if (btn) state = FM_DISPLAY;
        break;
      case FM_DISPLAY:
        if (!btn) state = FM_WAIT;
        break;
      case FM_WAIT:
        if (btn) state = AM_DISPLAY;
        break;
    }

    switch(state) {
      case AM_DISPLAY:
      case AM_WAIT:
        value = map(ADC_read(0), 0, 1023, 0, 9);
        break;
      case FM_DISPLAY:
      case FM_WAIT:
        value = map(ADC_read(0), 0, 1023, 10, 15);
        break;
    }

    displayNum(value);

    if (((state==AM_DISPLAY||state==AM_WAIT) && value==4) ||
        ((state==FM_DISPLAY||state==FM_WAIT) && value==14)) {
        PORTD = SetBit(PORTD, PD2, 1);
    } else {
        PORTD = SetBit(PORTD, PD2, 0);
    }
}

int main(void) {

    DDRB  = 0x1F;
    PORTB = 0x00;

    DDRC  = 0x00;
    PORTC = (1<<PC1);

    DDRD  = (1<<PD2)|(1<<PD6)|(1<<PD7);
    PORTD = 0x00;

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
