/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 6  Exercise 3


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: https://youtu.be/N4k5Jzb98eo

 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include "timerISR.h" 

int phases[8] = {0b0001,0b0011,0b0010,0b0110,0b0100,0b1100,0b1000,0b1001}; 

unsigned char direction = 0;      
unsigned char buffer[4];          
unsigned char bufIdx = 0;         
unsigned char stepCount = 0;      
unsigned char locked = 1;         
signed char motorDir = 1;       
unsigned char lockTrigger = 0;    
unsigned char motorBusy = 0;      
unsigned char errorFlag = 0;      
unsigned long errorTime = 0;      
unsigned long blinkTimer = 0;     

unsigned char changingPasscode = 0;       
volatile unsigned char debouncedJoyButtonPressed = 0; 

unsigned char passcode[4] = {1,3,4,2}; 

void ADC_init() {
    ADMUX  = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 
}

unsigned int ADC_read(uint8_t ch) {
    ch &= 0x07; 
    ADMUX = (ADMUX & 0xF8) | ch; 
    ADCSRA |= (1 << ADSC); 
    while (ADCSRA & (1 << ADSC)); 
    return ADCW; 
}

#define NUM_TASKS 6 
const unsigned long TASK0_PERIOD = 50;
const unsigned long TASK1_PERIOD = 100;
const unsigned long TASK2_PERIOD = 50;
const unsigned long TASK3_PERIOD = 50;
const unsigned long TASK4_PERIOD = 10;
const unsigned long TASK5_PERIOD = 20;
const unsigned long GCD_PERIOD   = 10;

#define PHASE_INC 2 

typedef struct _task {
    signed   char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;
task tasks[NUM_TASKS];

#define VRX_CH   PC2  
#define VRY_CH   PC3  

#define SW_PORT_REG  PINC   
#define SW_PIN       PC4    
#define SW_BIT       (1 << SW_PIN)

#define LED_PORT     PORTC
#define LED_DDR      DDRC
#define LED_MASK     0x03   

#define DP_PORT_REG PORTB
#define DP_PIN      PB1 
#define DP_BIT      (1 << DP_PIN)

const uint8_t segMap[5] = {
   0b00000001, 
    0b01111100, 
    0b00001001, 
    0b01111001, 
    0b00011100
};

enum T0_States { T0_READ_JOYSTICK };
enum T1_States { T1_DISPLAY_DIRECTION };
enum T2_States { T2_WAIT_INPUT, T2_WAIT_JOYSTICK_RELEASE, T2_PROCESS_PASSCODE_ATTEMPT };
enum T3_States { T3_UPDATE_LEDS };
enum T4_States { T4_MOTOR_IDLE, T4_MOTOR_RUN };
enum T5_States { T5_BTN_RELEASED, T5_BTN_MAYBE_PRESSED, T5_BTN_PRESSED, T5_BTN_MAYBE_RELEASED };

int TickFct_Task0_ReadJoystick(int state) {
    unsigned int vx = ADC_read(VRX_CH); 
    unsigned int vy = ADC_read(VRY_CH); 

    if (vx > 750)      direction = 1; 
    else if (vx < 250) direction = 3; 
    else if (vy > 750) direction = 2; 
    else if (vy < 250) direction = 4; 
    else               direction = 0; 

    return T0_READ_JOYSTICK;
}

int TickFct_Task1_Display(int state) {
    uint8_t seg_pattern = segMap[direction];
    uint8_t portb_val = PORTB & 0xFC; 
    uint8_t portd_val = PORTD & 0x03; 

    if (seg_pattern & 0x01) { 
        portb_val |= (1 << PB0);
    }

    portd_val |= (seg_pattern & 0xFC); 

    if (changingPasscode) {
        portb_val |= DP_BIT;  
    }

    PORTB = portb_val;
    PORTD = portd_val;
    
    return T1_DISPLAY_DIRECTION;
}

int TickFct_Task2_PasscodeLogic(int state) {
    static unsigned char prev_joy_button_state_task2 = 0;
    unsigned char current_joy_button_state_task2 = debouncedJoyButtonPressed; 
    unsigned char button_press_event_task2 = current_joy_button_state_task2 && !prev_joy_button_state_task2;
    prev_joy_button_state_task2 = current_joy_button_state_task2;

    switch (state) {
        case T2_WAIT_INPUT:
            stepCount = bufIdx; 
            
            if (!locked && !changingPasscode && button_press_event_task2) {
                changingPasscode = 1; 
                bufIdx = 0;           
                stepCount = 0;        
            }
            else if (direction != 0) {
                if (bufIdx < 4) { 
                    buffer[bufIdx] = direction;
                    state = T2_WAIT_JOYSTICK_RELEASE;
                }
            }
            break;

        case T2_WAIT_JOYSTICK_RELEASE:
            if (direction == 0) { 
                if (bufIdx < 4) { 
                    bufIdx++;
                    stepCount = bufIdx; 
                }

                if (bufIdx >= 4) { 
                    state = T2_PROCESS_PASSCODE_ATTEMPT;
                } else {
                    state = T2_WAIT_INPUT; 
                }
            }
            break;

        case T2_PROCESS_PASSCODE_ATTEMPT:
            if (changingPasscode) {
                for (uint8_t i = 0; i < 4; ++i) {
                    passcode[i] = buffer[i];
                }
                changingPasscode = 0; 
            } else { 
                unsigned char match = 1;
                for (uint8_t i = 0; i < 4; ++i) {
                    if (buffer[i] != passcode[i]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    locked = !locked; 
                    motorDir = locked ? -1 : 1; 
                    lockTrigger = 1;    
                    errorFlag = 0;      
                } else {
                    errorFlag = 1;      
                    errorTime = 0;      
                    blinkTimer = 0;     
                }
            }
            bufIdx = 0;     
            stepCount = 0;  
            state = T2_WAIT_INPUT;
            break;

        default: 
            state = T2_WAIT_INPUT;
            break;
    }
    return state;
}

int TickFct_Task3_UpdateLeds(int state) {
    static unsigned char error_blink_led_state_value = 0; 
    unsigned char current_led_pattern_on_portc = 0; 

    if (motorBusy) {
        current_led_pattern_on_portc = LED_MASK; 
    } else if (errorFlag) {
        errorTime += TASK3_PERIOD;
        blinkTimer += TASK3_PERIOD;

        if (blinkTimer >= 500) { 
            blinkTimer = 0; 
            error_blink_led_state_value = (error_blink_led_state_value == 0) ? LED_MASK : 0; 
        }
        current_led_pattern_on_portc = error_blink_led_state_value;

        if (errorTime >= 4000) { 
            errorFlag = 0;
            current_led_pattern_on_portc = 0; 
            error_blink_led_state_value = 0; 
        }
    } else { 
        if (stepCount >= 1 && stepCount <= 3) {
            current_led_pattern_on_portc = (stepCount & LED_MASK);
        } else {
            current_led_pattern_on_portc = 0; 
        }
    }

    LED_PORT = (LED_PORT & ~LED_MASK) | current_led_pattern_on_portc;

    return T3_UPDATE_LEDS; 
}

int TickFct_Task4_MotorControl(int state) {
    static int current_phase_index = 0;
    static int steps_to_take = 0;

    switch (state) {
        case T4_MOTOR_IDLE:
            if (lockTrigger) {
                lockTrigger = 0;
                motorBusy = 1;
                steps_to_take = 1024 / PHASE_INC; 
                state = T4_MOTOR_RUN;
            }
            break;

        case T4_MOTOR_RUN:
            if (steps_to_take > 0) {
                PORTB = (PORTB & 0xC3) | (phases[current_phase_index] << 2); 
                current_phase_index = (current_phase_index + (motorDir * PHASE_INC) + 8) % 8;
                steps_to_take--;
            }

            if (steps_to_take <= 0) {
                motorBusy = 0;
                state = T4_MOTOR_IDLE;
            }
            break;
        default:
            state = T4_MOTOR_IDLE;
            break;
    }
    return state;
}

int TickFct_Task5_DebounceButton(int state) {
    static unsigned char debounce_counter = 0;
    const unsigned char DEBOUNCE_SAMPLES_THRESHOLD = 2; 

    unsigned char raw_button_state = !(SW_PORT_REG & SW_BIT); 

    switch (state) {
        case T5_BTN_RELEASED:
            if (raw_button_state) { 
                state = T5_BTN_MAYBE_PRESSED;
                debounce_counter = 0;
            }
            break;

        case T5_BTN_MAYBE_PRESSED:
            if (raw_button_state) {
                debounce_counter++;
                if (debounce_counter >= DEBOUNCE_SAMPLES_THRESHOLD) {
                    state = T5_BTN_PRESSED;
                    debouncedJoyButtonPressed = 1; 
                }
            } else { 
                state = T5_BTN_RELEASED;
            }
            break;

        case T5_BTN_PRESSED:
            if (!raw_button_state) { 
                state = T5_BTN_MAYBE_RELEASED;
                debounce_counter = 0;
            }
            break;

        case T5_BTN_MAYBE_RELEASED:
            if (!raw_button_state) {
                debounce_counter++;
                if (debounce_counter >= DEBOUNCE_SAMPLES_THRESHOLD) {
                    state = T5_BTN_RELEASED;
                    debouncedJoyButtonPressed = 0; 
                }
            } else { 
                state = T5_BTN_PRESSED;
            }
            break;
        default: 
            state = T5_BTN_RELEASED; 
            break;
    }
    return state;
}

void TimerISR() {
    for (unsigned char i = 0; i < NUM_TASKS; ++i) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

int main(void) {
    LED_DDR |= LED_MASK;       
    PORTC = SW_BIT;            

    DDRB  = 0x3F; 
    PORTB = 0x00; 

    DDRD  = 0xFC; 
    PORTD = 0x00; 

    ADC_init(); 

    unsigned char i = 0;
    tasks[i].state = T0_READ_JOYSTICK;
    tasks[i].period = TASK0_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task0_ReadJoystick;
    i++;
    tasks[i].state = T1_DISPLAY_DIRECTION;
    tasks[i].period = TASK1_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task1_Display;
    i++;
    tasks[i].state = T2_WAIT_INPUT;
    tasks[i].period = TASK2_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task2_PasscodeLogic;
    i++;
    tasks[i].state = T3_UPDATE_LEDS;
    tasks[i].period = TASK3_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task3_UpdateLeds;
    i++;
    tasks[i].state = T4_MOTOR_IDLE;
    tasks[i].period = TASK4_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task4_MotorControl;
    i++;
    tasks[i].state = T5_BTN_RELEASED; 
    tasks[i].period = TASK5_PERIOD;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Task5_DebounceButton;

    TimerSet(GCD_PERIOD);
    TimerOn();
    sei(); 

    while (1) { }
    return 0; 
}
