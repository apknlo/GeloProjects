/*        Angelo Torres & atorr369@ucr.edu:

*          Discussion Section: 023

 *         Assignment: Lab 2  Exercise 3

 *         Exercise Description: [optional - include for your own benefit]

 *        

 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.

 *

 *         Demo Link: 

 */

#include <avr/io.h>
#include <util/delay.h>

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
    return (b ? (x | (0x01 << k)) : (x & ~(0x01 << k)));
}

unsigned char GetBit(unsigned char x, unsigned char k) {
    return ((x & (0x01 << k)) != 0);
}

int nums[16] = {
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

void outNum(int num) {
    PORTB = nums[num] & 0x1F;
    PORTD = (PORTD & ~((1 << PD6) | (1 << PD7))) | ((nums[num] & 0x60) << 1);
}

void Tick() {
    static unsigned char count = 0;
   
    static unsigned char holdLeft = 0;
    static unsigned char holdRight = 0;
    
    unsigned char leftBPressed  = !(PINC & (1 << PC1));
    unsigned char rightBPressed = !(PINC & (1 << PC0));
    
    if (leftBPressed && !holdLeft) {
        count++;
        if (count > 15) {
            count = 0;
        }
    }
    
    if (rightBPressed && !holdRight) {
        if (count == 0)
            count = 15;
        else
            count--;
    }
    
    unsigned char ledOut = (count & 0x0F) << 2;
    
    PORTD = (PORTD & ~0x3C) | ledOut;
    
    
    outNum(count);
    
    
    holdLeft = leftBPressed;
    holdRight = rightBPressed;
    
    _delay_ms(500);  
}


int main(void)
{
    DDRB = 0x1F;      
    PORTB = 0x00;     
    
    DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    PORTD = 0x00;     
    
    DDRC &= ~((1 << PC0) | (1 << PC1)); 
    PORTC |= (1 << PC0) | (1 << PC1);     
    
    while (1) {
        Tick();
    }
    
    return 0;
}