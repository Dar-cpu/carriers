#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stdbool.h>

void board_init(void);
uint32_t board_millis(void);
void board_relay_set(bool on);
void board_buzzer_set(bool on);
#define LCD_PARALLEL 0U
#define LCD_I2C 1U
void board_lcd_init(void);
bool board_lcd_select(uint8_t mode, uint8_t address);
uint8_t board_lcd_mode(void);
uint8_t board_lcd_address(void);
bool board_lcd_healthy(void);
void board_lcd_set(const char *top, const char *bottom);
char board_keypad_poll(uint32_t now);
bool board_button1_pressed(uint32_t now);
bool board_button2_pressed(uint32_t now);
bool board_uart_get(uint8_t *out);
void board_uart_put(char c);

#endif
