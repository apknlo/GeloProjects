#include <avr/io.h>
#include <util/delay.h>

#include <avr/io.h>
#include <util/delay.h>

//------------------------------------------------------------
// State machine states.
enum states { WAIT, LEFT_PRESSED, RIGHT_PRESSED } state;

//------------------------------------------------------------
// The nums array for the 1D7S display.
// Bits layout (LSB to MSB): bit0 = a, bit1 = b, bit2 = c,
// bit3 = d, bit4 = e, bit5 = f, bit6 = g.
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
    0b1110111, // A (10)
    0b1111100, // b (11)
    0b0111001, // C (12)
    0b1011110, // d (13)
    0b1111001, // E (14)
    0b1110001  // F (15)
};

//------------------------------------------------------------
// Tick() implements the state machine for button edge-detection,
// updates the count (wrapping from 15→0 and 0→15), and writes the output.
// It updates:
//   • The 4-LED binary display on PORTD (PD2–PD5)
//   • The 7‑segment display’s segments f & g on PORTD (PD6–PD7)
//   • The 7‑segment display’s segments a–e on PORTB (PB0–PB4)
void Tick(void) {
    static unsigned char count = 0;  // 4-bit count (0–15)

    // Read button states from PINC.
    // (Buttons are active low because internal pull-ups are enabled.)
    // Left button (increment) is on PC1; right button (decrement) is on PC0.
    unsigned char left  = !(PINC & (1 << PC1));
    unsigned char right = !(PINC & (1 << PC0));

    // --- State Machine ---
    switch(state) {
        case WAIT:
            if (left && !right) {  
                // Left button pressed: increment the count.
                count = (count + 1) & 0x0F;  // Wraps: 15 + 1 → 0.
                state = LEFT_PRESSED;
            } 
            else if (right && !left) {  
                // Right button pressed: decrement the count.
                count = (count == 0) ? 15 : count - 1;
                state = RIGHT_PRESSED;
            }
            break;

        case LEFT_PRESSED:
            // Wait until the left button is released.
            if (!left)
                state = WAIT;
            break;

        case RIGHT_PRESSED:
            // Wait until the right button is released.
            if (!right)
                state = WAIT;
            break;

        default:
            state = WAIT;
            break;
    }

    // --- Update Outputs ---

    // (1) For the 4-LED display (binary count) on PD2–PD5:
    //     Shift the lower 4 bits of count left by 2 to align with PD2–PD5.
    unsigned char ledPart = (count & 0x0F) << 2;

    // (2) For the 7-segment display's segments f and g (on PD6 and PD7):
    //     Extract bits 5 and 6 from the pattern in nums[count] (mask 0x60)
    //     and shift left by 1 so that bit5 goes to PD6 and bit6 goes to PD7.
    unsigned char segFG = (nums[count] & 0x60) << 1;

    // Combine the LED portion and the 7-seg (f, g) portion into a single value.
    unsigned char combinedPORTD = ledPart | segFG;
    PORTD = combinedPORTD;  // Update PD2–PD7 in one write.

    // (3) For the 7-segment display's segments a–e (on PB0–PB4):
    PORTB = nums[count] & 0x1F;

    _delay_ms(50);  // Delay for button debouncing.
}

//------------------------------------------------------------
// main() configures I/O ports according to your setup and enters the main loop.
int main(void) {
    // --- Button Setup (PORTC) ---
    // PC0: Right button (decrement), PC1: Left button (increment).
    DDRC &= ~((1 << PC0) | (1 << PC1));  // Set PC0 and PC1 as inputs.
    PORTC |= (1 << PC0) | (1 << PC1);      // Enable internal pull-up resistors.

    // --- 7-Segment Display Setup for segments a–e (PORTB) ---
    // PB0–PB4 (segments a–e) are outputs.
    DDRB |= 0x1F;  // 0b00011111: PB0 to PB4 as outputs.
    PORTB = 0;

    // --- LED & 7-Segment (f, g) Setup (PORTD) ---
    // 4-LEDs are connected to PD2–PD5.
    // 7-seg segments f and g are connected to PD6–PD7.
    DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    PORTD = 0;

    // Initialize the state machine.
    state = WAIT;

    // Main loop.
    while (1) {
        Tick();
    }

    return 0;
}

