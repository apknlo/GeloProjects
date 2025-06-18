/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 3  Exercise 2


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/Ph3MrSNXE8Y


 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "timerISR.h"

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
    return (b ? (x | (1<<k)) : (x & ~(1<<k)));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
    return ((x & (1<<k)) != 0);
}

// segments bits 0–5 = A–F, bit 6 = G
const unsigned char nums[16] = {
    0b0111111, //0
    0b0000110, //1
    0b1011011, //2
    0b1001111, //3
    0b1100110, //4
    0b1101101, //5
    0b1111101, //6
    0b0000111, //7
    0b1111111, //8
    0b1101111, //9
    0b1110111, //A
    0b1111100, //b
    0b0111001, //C
    0b1011110, //d
    0b1111001, //E
    0b1110001  //F
};

void outNum(unsigned char idx) {
    // drive segments A–F on PB0–PB5
    PORTB = nums[idx] & 0x3F;

    // build PORTD: preserve RGB bits (PD2–PD4)
    uint8_t pd = PORTD & ((1<<PD2)|(1<<PD3)|(1<<PD4));

    // segment G → PD7
    if (nums[idx] & (1<<6)) pd |= (1<<PD7);

    // digit-select: D4=PD5 low (ON), D1=PD6 high (OFF)
    pd &= ~(1<<PD5);
    pd |=  (1<<PD6);

    PORTD = pd;
}

void ADC_init(void) {
    ADMUX  = (1<<REFS0);                                // AVCC ref, right-adjust
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); // enable, prescale=128
    ADCSRB = (1<<ACME);                                 // per pre-lab
}

unsigned int ADC_read(unsigned char chnl){
    ADMUX  = (ADMUX & 0xF0) | (chnl & 0x07);  // select channel
    ADCSRA |= (1<<ADSC);                      // start conversion
    while (ADCSRA & (1<<ADSC));               // wait
    uint8_t low  = ADCL;
    uint8_t high = ADCH;
    return (high<<8) | low;
}

// integer‐only map()
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min)*(out_max - out_min)/(in_max - in_min) + out_min;
}

enum states { DISPLAY } state = DISPLAY;

void Tick(void) {
    static unsigned char value;

    // transitions (single‐state SM)
    switch(state) {
        case DISPLAY:
        default:
            state = DISPLAY;
            break;
    }

    // actions
    switch(state) {
        case DISPLAY:
            // read pot on PC1, map to 0–9
            value = map(ADC_read(1), 0, 1023, 0, 9);
            outNum(value);

            // RGB LED logic: preserve digit‐select & G bits
            {
                uint8_t pd = PORTD & ((1<<PD5)|(1<<PD6)|(1<<PD7));
                uint8_t leds = 0;

                if (value <= 3) {
                    leds = (1<<PD2);               // blue
                } else if (value < 8) {
                    leds = (1<<PD3)|(1<<PD4);      // green+red = yellow
                } else {
                    leds = (1<<PD4);               // red
                }

                PORTD = pd | leds;
            }
            break;
    }
}

int main(void)
{
    // PB0–PB5 = segments A–F
    DDRB  = 0x3F;
    PORTB = 0x00;

    // PC0 = photo resistor (unused in ex1), PC1=pot, PC2=button
    DDRC  = 0x00;
    PORTC = (1<<PC2);   // pull‐up on button

    // PD2=blue, PD3=green, PD4=red, PD5=D4, PD6=D1, PD7=G
    DDRD  = (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7);
    PORTD = 0x00;

    ADC_init();

    TimerSet(500);   // 500 ms SynchSM
    TimerOn();

    while (1) {
        Tick();
        while (!TimerFlag) { }
        TimerFlag = 0;
    }
    return 0;
}


