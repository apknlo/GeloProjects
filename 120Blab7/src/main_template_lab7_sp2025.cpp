/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Lab 7  Exercise 3


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: 

 */

#include "timerISR.h"
#include "helper.h" 
#include "periph.h"

#define NUM_TASKS 4 

typedef struct _task {
    signed char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

// Task Periods
const unsigned long GCD_PERIOD                 = 1;    
const unsigned long IND_PERIOD                 = 400;
const unsigned long HORN_PERIOD                = 50;
const unsigned long STEPPER_MOTOR_TASK_PERIOD  = 1;    
const unsigned long STEERING_SERVO_TASK_PERIOD = 20;   

task tasks[NUM_TASKS];

void TimerISR() {
    for (unsigned int i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime++;
    }
}

#define LEDL1 PD5
#define LEDL2 PD7
#define LEDL3 PB0
#define LEDR1 PD4
#define LEDR2 PD3
#define LEDR3 PD2

#define BTN_LEFT_PIN  PC4
#define BTN_RIGHT_PIN PC3
#define JOY_BTN_PIN   PC2

#define BTN_LEFT()  (!(PINC & (1 << BTN_LEFT_PIN)))
#define BTN_RIGHT() (!(PINC & (1 << BTN_RIGHT_PIN)))
#define JOY_BTN()   (!(PINC & (1 << JOY_BTN_PIN)))

#define STEPPER_PIN1 PB5 
#define STEPPER_PIN2 PB4 
#define STEPPER_PIN3 PB3 
#define STEPPER_PIN4 PB2 
#define STEPPER_PORT PORTB
#define STEPPER_DDR  DDRB
#define ALL_STEPPER_PINS_MASK ((1 << STEPPER_PIN1) | (1 << STEPPER_PIN2) | (1 << STEPPER_PIN3) | (1 << STEPPER_PIN4))

#define JOYSTICK_X_ADC_CHANNEL 0  
#define JOYSTICK_Y_ADC_CHANNEL 1   


#define ADC_CENTER_VAL         512
#define ADC_DEADBAND           25  
#define ADC_MAX_INPUT_VAL      1023
#define ADC_MIN_INPUT_VAL      0

#define MAX_STEP_DELAY_MS 30 
#define MIN_STEP_DELAY_MS 2  

#define SERVO_PULSE_MIN_COUNTS  1000 
#define SERVO_PULSE_CENTER_COUNTS 3000 
#define SERVO_PULSE_MAX_COUNTS  5000 

#define HONK_OCR_VAL       128
#define BRAKE_BEEP_OCR_VAL 192
#define SILENT_OCR_VAL     255

enum IndStates{ IND_IDLE, IND_LEFT, IND_RIGHT };

static const uint8_t patL_D[4] = { 0,
    (1<<LEDL2),
    (1<<LEDL2)|(1<<LEDL1),
    0
};
static const uint8_t patL_B[4] = { (1<<LEDL3), (1<<LEDL3), (1<<LEDL3), 0 };

static const uint8_t patR_D[4] = { (1<<LEDR1),
    (1<<LEDR1)|(1<<LEDR2),
    (1<<LEDR1)|(1<<LEDR2)|(1<<LEDR3),
    0
};
int TickF_Indicator(int state){
    static uint8_t idx=0, prev=0;
    prev = state;
    
    if(BTN_LEFT() && !BTN_RIGHT())      state = IND_LEFT;
    else if(BTN_RIGHT() && !BTN_LEFT()) state = IND_RIGHT;
    else                                state = IND_IDLE;
    if(state != prev) idx = 0;

    PORTD &= ~((1<<LEDL1)|(1<<LEDL2)|(1<<LEDR1)|(1<<LEDR2)|(1<<LEDR3));
    PORTB &= ~(1<<LEDL3);

    if(state == IND_RIGHT){
        PORTD |= patL_D[idx];
        PORTB |= patL_B[idx];
    } else if(state == IND_LEFT) {
        PORTD |= patR_D[idx];
    }

    if(state != IND_IDLE) idx = (idx + 1) & 0x03;
    return state;
}
int TickF_Horn(int state) {
    if (JOY_BTN()) OCR0A = HONK_OCR_VAL;
    else { if (OCR0A == HONK_OCR_VAL) OCR0A = SILENT_OCR_VAL; }
    return state;
}
const uint8_t stepper_phases[] = {
    (1 << STEPPER_PIN1), (1 << STEPPER_PIN2), (1 << STEPPER_PIN3), (1 << STEPPER_PIN4)
};
#define NUM_STEPPER_PHASES (sizeof(stepper_phases) / sizeof(stepper_phases[0]))

enum StepperMotorState { SM_IDLE, SM_CW, SM_CCW }; 
enum BeeperFSMState { BEEP_FSM_IDLE, BEEP_FSM_ACTIVE, BEEP_FSM_SILENT_INTERVAL };

int TickF_StepperControl(int current_stepper_fsm_state) {
    static uint8_t current_phase_index = 0;
    static unsigned long step_timer_ms = 0;
    static unsigned long current_target_step_delay_ms = MAX_STEP_DELAY_MS;
    static uint8_t motor_should_be_active = 0;

    static enum BeeperFSMState beeper_state = BEEP_FSM_IDLE;
    static unsigned long beeper_cycle_timer_ms = 0;
    static uint8_t joystick_is_physically_down_for_brake = 0; 

    uint16_t adc_val_y; 
    long displacement_from_deadband_edge;

    long max_displacement_stepper_up = ADC_MAX_INPUT_VAL - (ADC_CENTER_VAL + ADC_DEADBAND);
    if (max_displacement_stepper_up <= 0) max_displacement_stepper_up = 1;

    long max_displacement_stepper_down = (ADC_CENTER_VAL - ADC_DEADBAND) - ADC_MIN_INPUT_VAL;
    if (max_displacement_stepper_down <= 0) max_displacement_stepper_down = 1;

    adc_val_y = ADC_read(JOYSTICK_Y_ADC_CHANNEL);

    joystick_is_physically_down_for_brake = 0; 
    if (adc_val_y > (ADC_CENTER_VAL + ADC_DEADBAND)) { 
        current_stepper_fsm_state = SM_CW; 
        displacement_from_deadband_edge = adc_val_y - (ADC_CENTER_VAL + ADC_DEADBAND);
        if (displacement_from_deadband_edge > max_displacement_stepper_up) displacement_from_deadband_edge = max_displacement_stepper_up;
        
        if (max_displacement_stepper_up > 0) {
            long delta_delay_total = (long)MIN_STEP_DELAY_MS - (long)MAX_STEP_DELAY_MS; 
            long scaled_delta = (displacement_from_deadband_edge * delta_delay_total) / max_displacement_stepper_up;
            current_target_step_delay_ms = (unsigned long)((long)MAX_STEP_DELAY_MS + scaled_delta);
        } else { 
            current_target_step_delay_ms = MAX_STEP_DELAY_MS; 
        }
        motor_should_be_active = 1;

    } else if (adc_val_y < (ADC_CENTER_VAL - ADC_DEADBAND)) { 
        current_stepper_fsm_state = SM_CCW; 
        displacement_from_deadband_edge = (ADC_CENTER_VAL - ADC_DEADBAND) - adc_val_y;
        if (displacement_from_deadband_edge > max_displacement_stepper_down) displacement_from_deadband_edge = max_displacement_stepper_down;

        if (max_displacement_stepper_down > 0) {
            long delta_delay_total = (long)MIN_STEP_DELAY_MS - (long)MAX_STEP_DELAY_MS;
            long scaled_delta = (displacement_from_deadband_edge * delta_delay_total) / max_displacement_stepper_down;
            current_target_step_delay_ms = (unsigned long)((long)MAX_STEP_DELAY_MS + scaled_delta);
        } else { 
            current_target_step_delay_ms = MAX_STEP_DELAY_MS; 
        }
        motor_should_be_active = 1;
        joystick_is_physically_down_for_brake = 1; 
    } else { 
        current_stepper_fsm_state = SM_IDLE;
        motor_should_be_active = 0;
        STEPPER_PORT &= ~ALL_STEPPER_PINS_MASK;
    }

    if (motor_should_be_active) {
        if (current_target_step_delay_ms < MIN_STEP_DELAY_MS) current_target_step_delay_ms = MIN_STEP_DELAY_MS;
        if (current_target_step_delay_ms > MAX_STEP_DELAY_MS) current_target_step_delay_ms = MAX_STEP_DELAY_MS;
    }

    step_timer_ms++;
    if (motor_should_be_active && (step_timer_ms >= current_target_step_delay_ms)) {
        step_timer_ms = 0;
        if (current_stepper_fsm_state == SM_CW) { 
            current_phase_index = (current_phase_index + 1) % NUM_STEPPER_PHASES;
        } else if (current_stepper_fsm_state == SM_CCW) { 
            if (current_phase_index == 0) current_phase_index = NUM_STEPPER_PHASES - 1;
            else current_phase_index--;
        }
        STEPPER_PORT = (STEPPER_PORT & ~ALL_STEPPER_PINS_MASK) | stepper_phases[current_phase_index];
    }

    beeper_cycle_timer_ms++;
    if (!joystick_is_physically_down_for_brake && beeper_state != BEEP_FSM_IDLE) {
        if (OCR0A == BRAKE_BEEP_OCR_VAL) OCR0A = SILENT_OCR_VAL;
        beeper_state = BEEP_FSM_IDLE;
    }
    switch (beeper_state) {
        case BEEP_FSM_IDLE:
            if (joystick_is_physically_down_for_brake) {
                beeper_state = BEEP_FSM_ACTIVE; OCR0A = BRAKE_BEEP_OCR_VAL; beeper_cycle_timer_ms = 0;
            } break;
        case BEEP_FSM_ACTIVE:
            if (beeper_cycle_timer_ms >= 1000) {
                OCR0A = SILENT_OCR_VAL; beeper_state = BEEP_FSM_SILENT_INTERVAL; beeper_cycle_timer_ms = 0;
            } break;
        case BEEP_FSM_SILENT_INTERVAL:
            if (beeper_cycle_timer_ms >= 1000) { beeper_state = BEEP_FSM_IDLE; }
            break;
    }
    return current_stepper_fsm_state;
}

enum SteeringState { STEER_CENTER, STEER_PHYSICAL_LEFT_SERVO_CCW, STEER_PHYSICAL_RIGHT_SERVO_CW };

int TickF_Steering(int state) {
    uint16_t adc = ADC_read(JOYSTICK_X_ADC_CHANNEL);
    unsigned int target = SERVO_PULSE_CENTER_COUNTS;

    if (adc < ADC_CENTER_VAL - ADC_DEADBAND) {
        state = STEER_PHYSICAL_LEFT_SERVO_CCW;
        long disp     = (ADC_CENTER_VAL - ADC_DEADBAND) - adc;
        long max_disp = (ADC_CENTER_VAL - ADC_DEADBAND) - ADC_MIN_INPUT_VAL;
        if (max_disp <= 0) max_disp = 1;
        long delta = disp * ((long)SERVO_PULSE_MAX_COUNTS - SERVO_PULSE_CENTER_COUNTS)
                     / max_disp;
        target = SERVO_PULSE_CENTER_COUNTS + delta;  
    }
    else if (adc > ADC_CENTER_VAL + ADC_DEADBAND) {
        state = STEER_PHYSICAL_RIGHT_SERVO_CW;
        long disp     = adc - (ADC_CENTER_VAL + ADC_DEADBAND);
        long max_disp = ADC_MAX_INPUT_VAL - (ADC_CENTER_VAL + ADC_DEADBAND);
        if (max_disp <= 0) max_disp = 1;
        long delta = disp * (SERVO_PULSE_CENTER_COUNTS - (long)SERVO_PULSE_MIN_COUNTS)
                     / max_disp;
        target = SERVO_PULSE_CENTER_COUNTS - delta; 
    }
    else {
        state = STEER_CENTER;
    }
    if (target < SERVO_PULSE_MIN_COUNTS) target = SERVO_PULSE_MIN_COUNTS;
    if (target > SERVO_PULSE_MAX_COUNTS) target = SERVO_PULSE_MAX_COUNTS;

    OCR1A = target;
    return state;
}

void initBuzzerPWM_local(void) {
    DDRD |= (1 << PD6);
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (TCCR0B & 0xF8) | 0x03;
    OCR0A = SILENT_OCR_VAL;
}

void initServoPWM_local(void) { 
    DDRB |= (1 << PB1); 
    TCCR1A = (1 << COM1A1) | (1 << WGM11); 
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); 
    ICR1 = 39999; 
    OCR1A = SERVO_PULSE_CENTER_COUNTS; 
}

int main(void) {
    DDRD |= (1 << LEDL1) | (1 << LEDL2) | (1 << LEDR1) | (1 << LEDR2) | (1 << LEDR3);
    DDRB |= (1 << LEDL3);

    DDRC &= ~((1 << BTN_LEFT_PIN) | (1 << BTN_RIGHT_PIN) | (1 << JOY_BTN_PIN) | (1 << PC0) | (1 << PC1)); 
    PORTC |= (1 << BTN_LEFT_PIN) | (1 << BTN_RIGHT_PIN) | (1 << JOY_BTN_PIN);

    STEPPER_DDR |= ALL_STEPPER_PINS_MASK;
    STEPPER_PORT &= ~ALL_STEPPER_PINS_MASK;

    ADC_init(); 
    initBuzzerPWM_local(); 
    initServoPWM_local();  

    unsigned char i = 0;
    tasks[i].period = IND_PERIOD; 
    tasks[i].state = IND_IDLE;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickF_Indicator;
    i++;

    tasks[i].period = HORN_PERIOD;
    tasks[i].state = 0;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickF_Horn;
    i++;

    tasks[i].period = STEPPER_MOTOR_TASK_PERIOD;
    tasks[i].state = SM_IDLE;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickF_StepperControl;
    i++;

    tasks[i].period = STEERING_SERVO_TASK_PERIOD;
    tasks[i].state = STEER_CENTER; 
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickF_Steering;

    TimerSet(GCD_PERIOD); 
    TimerOn();            

    while (1) {}
    return 0;
}







