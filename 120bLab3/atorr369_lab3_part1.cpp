/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 3  Exercise 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/ryruXlaCsww


 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "timerISR.h"
#include "serialATmega.h"


void ADC_init() {

    ADMUX  = (1<<REFS0);
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
    ADCSRB = (1<<ACME);
}

unsigned int ADC_read(unsigned char chnl){
    
    ADMUX = (ADMUX & 0xF0) | (chnl & 0x07);
    ADCSRA |= (1<<ADSC);

    while((ADCSRA >> 6)&0x01) {}

    uint8_t low, high;

    low  = ADCL;
    high = ADCH;

    return (high << 8) | low;
}

void Tick() {
  unsigned int val = ADC_read(0);
  char array[6];
  int  i = 0;
  if (val == 0) {
      array[i++] = '0';
  } else {
      char rev[6];
      int  j = 0;
      while (val && j < 5) {
          rev[j++] = '0' + (val % 10);
          val   /= 10;
      }
      while (j--) {
          array[i++] = rev[j];
      }
  }
  array[i] = '\0';

  serial_println(array);
}


int main(void)
{
    
    DDRC  = 0x00;
    PORTC = 0x00;

    DDRB  = 0x00;
    PORTB = 0x00;

    DDRD  = 0x00;
    PORTD = 0x00;

    ADC_init();
    serial_init(9600);

    TimerSet(500);
    TimerOn();

    while (1)
    {
        Tick();  
        while (!TimerFlag) {}
        TimerFlag = 0;
    }
    return 0;
}
