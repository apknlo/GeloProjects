#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>
#include <util/delay.h>

#define DATA_PORT	PORTD   
#define DATA_DDR	DDRD
#define CTL_PORT    PORTC   
#define CTL_DDR		DDRC

#define LCD_D4_PIN	PD4     
#define LCD_D5_PIN	PD5     
#define LCD_D6_PIN	PD6     
#define LCD_D7_PIN	PD7     

#define LCD_EN_PIN	PC4     
#define LCD_RS_PIN	PC5     

#define LCD_CMD_CLEAR_DISPLAY	          0x01
#define LCD_CMD_CURSOR_HOME		          0x02

#define LCD_CMD_DISPLAY_OFF                0x08
#define LCD_CMD_DISPLAY_NO_CURSOR          0x0c
#define LCD_CMD_DISPLAY_CURSOR_NO_BLINK    0x0E
#define LCD_CMD_DISPLAY_CURSOR_BLINK       0x0F

#define LCD_CMD_4BIT_2ROW_5X7              0x28
#define LCD_CMD_8BIT_2ROW_5X7              0x38

void lcd_pulse_enable() {
    CTL_PORT |= (1 << LCD_EN_PIN);  
    _delay_us(1);                   
    CTL_PORT &= ~(1 << LCD_EN_PIN); 
    _delay_us(100);                 
}

void lcd_send_nibble(uint8_t nibble) {
    DATA_PORT &= 0x0F; 
    DATA_PORT |= (nibble << 4); 
    lcd_pulse_enable();
}

void lcd_send_command(uint8_t command) {
    CTL_PORT &= ~(1 << LCD_RS_PIN); 
    
    lcd_send_nibble(command >> 4); 
    lcd_send_nibble(command & 0x0F); 
    _delay_ms(2); 
}

void lcd_write_character(char character) {
    CTL_PORT |= (1 << LCD_RS_PIN); 

    lcd_send_nibble(character >> 4);  
    lcd_send_nibble(character & 0x0F); 
    _delay_ms(1); 
}

void lcd_write_str(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        lcd_write_character(str[i]);
        i++;
    }
}

void lcd_init(void) {
    DATA_DDR |= (1 << LCD_D7_PIN) | (1 << LCD_D6_PIN) | (1 << LCD_D5_PIN) | (1 << LCD_D4_PIN);
    CTL_DDR |= (1 << LCD_EN_PIN) | (1 << LCD_RS_PIN);
    _delay_ms(20);
    CTL_PORT &= ~(1 << LCD_RS_PIN); 
    CTL_PORT &= ~(1 << LCD_EN_PIN);  
    lcd_send_nibble(0x03);
    _delay_ms(5); 
    lcd_send_nibble(0x03);
    _delay_us(150); 
    lcd_send_nibble(0x03);
    _delay_us(150);
    lcd_send_nibble(0x02);
    _delay_us(150);
    lcd_send_command(LCD_CMD_4BIT_2ROW_5X7);
    lcd_send_command(LCD_CMD_DISPLAY_OFF);
    lcd_send_command(LCD_CMD_CLEAR_DISPLAY);
    _delay_ms(5); 
    lcd_send_command(0x06);
    lcd_send_command(LCD_CMD_DISPLAY_NO_CURSOR);
}

void lcd_clear() {
    lcd_send_command(LCD_CMD_CLEAR_DISPLAY);
    _delay_ms(5); 
}

void lcd_goto_xy(uint8_t line, uint8_t pos) {
    uint8_t address = 0x80; 
    if (line == 1) {
        address = 0xC0;
    }
    lcd_send_command(address + pos);
    _delay_us(50);
}

#endif 