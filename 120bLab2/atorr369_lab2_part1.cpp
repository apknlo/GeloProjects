/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 2  Exercise 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/iyH_t6UpTVs


 */


#include <avr/io.h>
#include <util/delay.h>


int main(void)
{
    DDRB = 0x3C;
    DDRD &= ~((1 << PD6) | (1 << PD7));
    PORTD |= (1 << PD6) | (1 << PD7);


    unsigned char currLED = 0;
   
    unsigned char holdLeftBPressed = 0;
    unsigned char holdRightBPressed = 0;
   
    while (1)
    {
        unsigned char leftBPressed  = !(PIND & (1 << PD6));
        unsigned char rightBPressed = !(PIND & (1 << PD7));
       
       
        if (leftBPressed && !holdLeftBPressed)
        {
           
            if (currLED > 0)
            {
                currLED--;
            }
        }
       
        if (rightBPressed && !holdRightBPressed)
        {
           
            if (currLED < 3)
            {
                currLED++;
            }
        }
       
 
        PORTB = (1 << (currLED + 2));
       
        holdLeftBPressed = leftBPressed;
        holdRightBPressed = rightBPressed;
       
        _delay_ms(100);
    }
   
    return 0;
}
