


/*        Angelo Torres & atorr369@ucr.edu:

*          Discussion Section: 023

 *         Assignment: Lab 2  Exercise 2

 *         Exercise Description: [optional - include for your own benefit]

 *        

 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.

 *

 *         Demo Link: https://youtu.be/rSlwxBeMKNc

 */

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    DDRB = 0x3C;
    DDRD &= ~((1 << PD6) | (1 << PD7));
    PORTD |= (1 << PD6) | (1 << PD7);
    
    unsigned char count = 0;
    
    unsigned char prevLeftPressed = 0;
    unsigned char prevRightPressed = 0;
    
    while (1)
    {
        unsigned char leftPressed  = !(PIND & (1 << PD7));
        unsigned char rightPressed = !(PIND & (1 << PD6));
        
        if (leftPressed && !prevLeftPressed)
        {
            count++;
            if (count > 15)
            {
                count = 0; 
            }
        }
       
        if (rightPressed && !prevRightPressed)
        {
            if (count == 0)
            {
                count = 15; 
            }
            else
            {
                count--;
            }
        }
        PORTB = (count << 2);
        prevLeftPressed = leftPressed;
        prevRightPressed = rightPressed;

        _delay_ms(100);
    }
    
    return 0;
}
