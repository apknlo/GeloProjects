/*        Angelo Torres & atorr369@ucr.edu:


*          Discussion Section: 023


 *         Assignment: Custom Lab Demo 1


 *         Exercise Description: [optional - include for your own benefit]


 *        


 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.


 *


 *         Demo Link: 

 */

#ifndef F_CPU
#define F_CPU 16000000UL 
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h> 
#include <stdio.h>  
#include <string.h> 
#include <avr/eeprom.h>

#include "LCD.h"      
#include "timerISR.h" 

#define NEXT_LETTER_PIN PD2
#define CONFIRM_SEL_PIN PD3
#define RESET_PIN       PC6
#define MORSE_LED_PIN   PB5
#define BUZZER_PIN      PB4
#define HC595_DS_PIN    PB0
#define HC595_SHCP_PIN  PB1
#define HC595_STCP_PIN  PB2
#define HC595_OE_PIN    PB3
#define SEG_D1_PIN      PC0
#define SEG_D2_PIN      PC1
#define SEG_D3_PIN      PC2
#define SEG_D4_PIN      PC3

#define DOT_DURATION   200
#define DASH_DURATION  (DOT_DURATION * 3)
#define INTER_ELEMENT_SPACE (DOT_DURATION)
#define SHORT_SPACE    (DOT_DURATION * 3)
#define BUZZER_FREQ    600

const char* MORSE_ALPHABET[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....", "..",   ".---",
    "-.-",  ".-..", "--",   "-.",   "---",  ".--.", "--.-", ".-.",  "...",  "-",
    "..-",  "...-", ".--",  "-..-", "-.--", "--.."
};
const char ALPHABET[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

typedef enum {
    INIT_GAME,
    NEW_ROUND,
    TRANSMIT_MORSE,
    AWAIT_PLAYER_INPUT,
    EVALUATE_ANSWER,
    DISPLAY_RESULT,
    GAME_OVER_SEQUENCE,
    DISPLAY_HIGH_SCORE
} GameState;

GameState current_game_state = INIT_GAME;
uint8_t current_round = 0;
uint8_t score = 0;
uint8_t high_score EEMEM = 0;
uint8_t current_target_char_index;
uint8_t player_selected_char_index = 0;

uint8_t seven_segment_digits[4] = {0, 0, 0, 0};
volatile uint8_t current_7seg_digit_mux = 0;
uint8_t display_result_flag = 0; 

const uint8_t seven_segment_map[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 
    0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 
};
#define SEG_BLANK 10

void setup_pins(void);
void setup_timers(void);
void handle_buttons(void);
void update_seven_segment_display(uint16_t value, uint8_t display_mode);
void drive_7seg_mux(void);
void hc595_send_byte(uint8_t data);
void play_morse_char(uint8_t char_index);
void play_morse_element(char element_type); 
void delay_us_var(uint16_t us); 
void play_tone(uint16_t frequency, uint16_t duration_ms);
void stop_tone(void);
void play_jingle_correct(void);
void play_jingle_incorrect(void);
void update_lcd_message(const char* line1, const char* line2);
void generate_random_char_index(void);
uint8_t read_high_score_eeprom(void);
void write_high_score_eeprom(uint8_t new_score);
void define_custom_lcd_chars(void);

#define TICK_PERIOD_MS 3
void TimerISR() {
    TimerFlag = 1;
    drive_7seg_mux();
}

int main(void) {
    setup_pins();
    lcd_init();
    define_custom_lcd_chars();
    setup_timers();
    srand(TCNT1);
    sei();

    uint8_t stored_high_score = read_high_score_eeprom();
    char hs_str[17]; // Increased size slightly for safety
    sprintf(hs_str, "HS:%02u", stored_high_score);
    update_lcd_message("Morse Challenge!", hs_str);

    while (1) {
        handle_buttons();

        switch (current_game_state) {
            case INIT_GAME:
                lcd_clear();
                score = 0;
                current_round = 0;
                player_selected_char_index = 0;
                update_seven_segment_display(player_selected_char_index + 1, 1);
                update_lcd_message("Press Confirm", "To Start Round");
                break;

            case NEW_ROUND:
                lcd_clear();
                current_round++;
                if (current_round > 10) {
                    current_game_state = GAME_OVER_SEQUENCE;
                    break;
                }
                player_selected_char_index = 0;
                update_seven_segment_display(player_selected_char_index + 1, 1);
                char round_str[17];
                sprintf(round_str, "Round: %u/10", current_round);
                update_lcd_message(round_str, "Listen...");
                _delay_ms(1000);
                generate_random_char_index();
                current_game_state = TRANSMIT_MORSE;
                break;

            case TRANSMIT_MORSE:
                play_morse_char(current_target_char_index);
                update_lcd_message("Your Guess?", "");
                current_game_state = AWAIT_PLAYER_INPUT;
                break;

            case AWAIT_PLAYER_INPUT:
                break;

            case EVALUATE_ANSWER:
                if (player_selected_char_index == current_target_char_index) {
                    score++;
                    display_result_flag = 1; 
                } else {
                    display_result_flag = 2; 
                }
                update_seven_segment_display(score, 0);
                current_game_state = DISPLAY_RESULT;
                break;
            
            case DISPLAY_RESULT:
                if (display_result_flag == 1) {
                    char correct_msg[17];
                    sprintf(correct_msg, "Correct! (%c)", ALPHABET[current_target_char_index]);
                    update_lcd_message(correct_msg, "");
                    play_jingle_correct();
                } else if (display_result_flag == 2) {
                    char wrong_msg[17];
                    sprintf(wrong_msg, "Wrong! Was: %c", ALPHABET[current_target_char_index]);
                    update_lcd_message(wrong_msg, "");
                    play_jingle_incorrect();
                }
                display_result_flag = 0;
                _delay_ms(1500);
                current_game_state = NEW_ROUND;
                break;

            case GAME_OVER_SEQUENCE:
                { 
                    char final_msg1[17];
                    char final_msg2[17];
                    uint8_t current_hs = read_high_score_eeprom();

                    if (score >= 7) {
                        strcpy(final_msg1, "YOU WIN!");
                        strcpy(final_msg2, "Congrats!");
                    } else {
                        strcpy(final_msg1, "GAME OVER");
                        strcpy(final_msg2, "Try Again.");
                    }
                    update_lcd_message(final_msg1, final_msg2);
                    _delay_ms(2000);

                    if (score > current_hs) {
                        write_high_score_eeprom(score);
                        _delay_ms(20); 
                        sprintf(final_msg2, "New High: %02u", score);
                        update_lcd_message(final_msg1, final_msg2);
                        _delay_ms(2000);
                    }
                    current_game_state = DISPLAY_HIGH_SCORE;
                }
                break;

            case DISPLAY_HIGH_SCORE:
                {
                    char final_score_msg[17];
                    char final_hs_msg[17];
                    sprintf(final_score_msg, "Score: %02u", score);
                    sprintf(final_hs_msg, "High Score: %02u", read_high_score_eeprom());
                    update_lcd_message(final_score_msg, final_hs_msg);
                }
                break;
        }

        while (!TimerFlag);
        TimerFlag = 0;
    }
    return 0;
}

void setup_pins(void) {
    DDRD &= ~((1 << NEXT_LETTER_PIN) | (1 << CONFIRM_SEL_PIN));
    PORTD |= (1 << NEXT_LETTER_PIN) | (1 << CONFIRM_SEL_PIN);

    DDRB |= (1 << MORSE_LED_PIN);
    DDRB |= (1 << BUZZER_PIN); 
    PORTB &= ~(1 << BUZZER_PIN); 

    DDRB |= (1 << HC595_DS_PIN) | (1 << HC595_SHCP_PIN) | (1 << HC595_STCP_PIN) | (1 << HC595_OE_PIN);
    PORTB |= (1 << HC595_OE_PIN);

    DDRC |= (1 << SEG_D1_PIN) | (1 << SEG_D2_PIN) | (1 << SEG_D3_PIN) | (1 << SEG_D4_PIN);
    PORTC |= (1 << SEG_D1_PIN) | (1 << SEG_D2_PIN) | (1 << SEG_D3_PIN) | (1 << SEG_D4_PIN); 
}

void setup_timers(void) {
    TimerSet(TICK_PERIOD_MS);
    TimerOn();
}

uint8_t next_button_state = 1;
uint8_t confirm_button_state = 1;
#define DEBOUNCE_TICKS 17

void handle_buttons(void) {
    static uint8_t next_debounce_counter = 0;
    static uint8_t confirm_debounce_counter = 0;
    static uint8_t last_next_button_val = 1;
    static uint8_t last_confirm_button_val = 1;

    uint8_t current_next_val = (PIND & (1 << NEXT_LETTER_PIN)) ? 1 : 0;
    uint8_t current_confirm_val = (PIND & (1 << CONFIRM_SEL_PIN)) ? 1 : 0;

    if (current_next_val != last_next_button_val) {
        next_debounce_counter = DEBOUNCE_TICKS;
    }
    if (next_debounce_counter > 0) {
        next_debounce_counter--;
        if (next_debounce_counter == 0) {
            if (current_next_val != next_button_state) {
                next_button_state = current_next_val;
                if (next_button_state == 0) { 
                    if (current_game_state == AWAIT_PLAYER_INPUT || current_game_state == INIT_GAME) {
                        player_selected_char_index = (player_selected_char_index + 1) % 26;
                        update_seven_segment_display(player_selected_char_index + 1, 1);
                    } else if (current_game_state == DISPLAY_HIGH_SCORE) {
                        current_game_state = INIT_GAME;
                    }
                }
            }
        }
    }
    last_next_button_val = current_next_val;

    if (current_confirm_val != last_confirm_button_val) {
        confirm_debounce_counter = DEBOUNCE_TICKS;
    }
    if (confirm_debounce_counter > 0) {
        confirm_debounce_counter--;
        if (confirm_debounce_counter == 0) {
            if (current_confirm_val != confirm_button_state) {
                confirm_button_state = current_confirm_val;
                if (confirm_button_state == 0) { 
                    if (current_game_state == INIT_GAME) {
                        current_game_state = NEW_ROUND;
                    } else if (current_game_state == AWAIT_PLAYER_INPUT) {
                        current_game_state = EVALUATE_ANSWER;
                    } else if (current_game_state == DISPLAY_HIGH_SCORE) {
                        current_game_state = INIT_GAME;
                    }
                }
            }
        }
    }
    last_confirm_button_val = current_confirm_val;
}

void hc595_send_byte(uint8_t data) {
    PORTB &= ~(1 << HC595_STCP_PIN);
    for (int8_t i = 7; i >= 0; i--) {
        PORTB &= ~(1 << HC595_SHCP_PIN);
        if ((data >> i) & 0x01) {
            PORTB |= (1 << HC595_DS_PIN);
        } else {
            PORTB &= ~(1 << HC595_DS_PIN);
        }
        PORTB |= (1 << HC595_SHCP_PIN);
    }
    PORTB |= (1 << HC595_STCP_PIN);
}

void update_seven_segment_display(uint16_t value, uint8_t display_mode) {
    if (display_mode == 0) { 
        value = (value > 99) ? 99 : value;
        seven_segment_digits[0] = SEG_BLANK;
        seven_segment_digits[1] = SEG_BLANK;
        seven_segment_digits[2] = (value / 10);
        seven_segment_digits[3] = value % 10;
        if (value < 10 && seven_segment_digits[2] == 0) seven_segment_digits[2] = SEG_BLANK;
    } else { 
        value = (value == 0) ? 26 : value;
        value = (value > 26) ? 1 : value;
        seven_segment_digits[0] = SEG_BLANK;
        seven_segment_digits[1] = SEG_BLANK;
        seven_segment_digits[2] = (value / 10);
        seven_segment_digits[3] = value % 10;
        if (value < 10 && seven_segment_digits[2] == 0) seven_segment_digits[2] = SEG_BLANK;
    }
}

void drive_7seg_mux(void) {
    PORTC |= (1 << SEG_D1_PIN) | (1 << SEG_D2_PIN) | (1 << SEG_D3_PIN) | (1 << SEG_D4_PIN);
    PORTB |= (1 << HC595_OE_PIN);

    hc595_send_byte(seven_segment_map[seven_segment_digits[current_7seg_digit_mux]]);

    PORTB &= ~(1 << HC595_OE_PIN);

    switch (current_7seg_digit_mux) {
        case 0: PORTC &= ~(1 << SEG_D1_PIN); break;
        case 1: PORTC &= ~(1 << SEG_D2_PIN); break;
        case 2: PORTC &= ~(1 << SEG_D3_PIN); break;
        case 3: PORTC &= ~(1 << SEG_D4_PIN); break;
    }
    current_7seg_digit_mux = (current_7seg_digit_mux + 1) % 4;
}

void delay_us_var(uint16_t us) {
    while (us > 0) {
        _delay_us(1); 
        us--;
    }
}

void play_tone(uint16_t frequency, uint16_t duration_ms) {
    if (frequency == 0 || duration_ms == 0) {
        if (duration_ms > 0) { 
             for(uint16_t i=0; i < duration_ms; ++i) _delay_ms(1);
        }
        return;
    }

    long half_period_us = 500000L / frequency; 
    if (half_period_us == 0) half_period_us = 1; 

    long cycles = (long)duration_ms * 1000L / (half_period_us * 2L);
    if (cycles == 0 && duration_ms > 0) cycles = 1;

    DDRB |= (1 << BUZZER_PIN); 

    for (long i = 0; i < cycles; i++) {
        PORTB |= (1 << BUZZER_PIN);
        delay_us_var(half_period_us); 
        PORTB &= ~(1 << BUZZER_PIN);
        delay_us_var(half_period_us); 
    }
    
    PORTB &= ~(1 << BUZZER_PIN);
}


void stop_tone(void) {
    PORTB &= ~(1 << BUZZER_PIN);
}

void play_morse_element(char element_type) {
    uint16_t duration = (element_type == '.') ? DOT_DURATION : DASH_DURATION;
    PORTB |= (1 << MORSE_LED_PIN);
    play_tone(BUZZER_FREQ, duration);
    PORTB &= ~(1 << MORSE_LED_PIN);
}

void play_morse_char(uint8_t char_index) {
    const char* morse_str = MORSE_ALPHABET[char_index];
    for (uint8_t i = 0; morse_str[i] != '\0'; i++) {
        play_morse_element(morse_str[i]);
        if (morse_str[i+1] != '\0') {
            _delay_ms(INTER_ELEMENT_SPACE);
        }
    }
    _delay_ms(SHORT_SPACE);
}

void play_jingle_correct(void) {
    play_tone(800, 150); _delay_ms(50);
    play_tone(1000, 150); _delay_ms(50);
    play_tone(1200, 200);
}

void play_jingle_incorrect(void) {
    play_tone(400, 200); _delay_ms(50);
    play_tone(300, 300);
}

void update_lcd_message(const char* line1, const char* line2) {
    lcd_goto_xy(0, 0);
    char display_line1[17]; 
    snprintf(display_line1, sizeof(display_line1), "%-16s", line1);
    lcd_write_str(display_line1);

    if (line2) { 
        lcd_goto_xy(1, 0);
        char display_line2[17];
        snprintf(display_line2, sizeof(display_line2), "%-16s", line2); 
        lcd_write_str(display_line2);
    } else { 
        lcd_goto_xy(1, 0);
        lcd_write_str("                "); 
    }
}

void generate_random_char_index(void) {
    current_target_char_index = rand() % 26;
}

uint8_t read_high_score_eeprom(void) {
    return eeprom_read_byte(&high_score);
}

void write_high_score_eeprom(uint8_t new_score) {
    eeprom_update_byte(&high_score, new_score);
}

const uint8_t custom_dot[8] = { 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00 };
const uint8_t custom_dash[8] = { 0x00, 0x00, 0x0E, 0x0E, 0x00, 0x00, 0x00, 0x00 };

void define_custom_lcd_chars(void) {
    lcd_send_command(0x40);
    for (int i = 0; i < 8; i++) lcd_write_character(custom_dot[i]);
    lcd_send_command(0x48);
    for (int i = 0; i < 8; i++) lcd_write_character(custom_dash[i]);
    lcd_send_command(LCD_CMD_CURSOR_HOME);
}