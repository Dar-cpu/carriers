/* LCD 16x2: parallel RB10..RB15 or PCF8574 on J14 RB8=SCL/RB9=SDA.
 * I2C1 HAL owns the shared bus on FJ/EP.
 * Backpack mapping: P0=RS P1=RW P2=EN P3=BL P4..P7=D4..D7.
 */
#include <xc.h>
#include "access_config.h"
#include <libpic30.h>
#include <string.h>
#include "board.h"
#include "i2c_bus.h"

static uint8_t mode = LCD_PARALLEL, address = ACCESS_LCD_I2C_ADDRESS;
static bool healthy = true, rs;
static char rows[2][17];

static bool pcf_nibble(uint8_t n)
{
    uint8_t data = (uint8_t)((n << 4) | 0x08U | (rs ? 1U : 0U));
    uint8_t tx[3]; bool ok;
    tx[0]=data; tx[1]=(uint8_t)(data|4U); tx[2]=data;
    ok=i2c_bus_transfer(address,tx,3,0,0);
    __delay_us(50); return ok;
}
static void nibble(uint8_t n)
{
    if (!healthy) return;
    if (mode == LCD_I2C) { healthy = pcf_nibble(n & 15U); return; }
    LATB = (uint16_t)((LATB & 0x0FFFU) | (((uint16_t)n & 15U) << 12));
    LATBbits.LATB10 = rs ? 1 : 0;
    LATBbits.LATB11 = 1; __delay_us(2);
    LATBbits.LATB11 = 0; __delay_us(50);
}
static void lcd_byte(uint8_t value, bool data)
{
    rs = data; nibble(value >> 4); nibble(value);
    if (!data && (value == 1U || value == 2U)) __delay_ms(2);
}
static void initialize(void)
{
    rs = false;
    __delay_ms(30);
    nibble(3); __delay_ms(5);
    nibble(3); __delay_us(160);
    nibble(3); __delay_ms(2);
    nibble(2); __delay_ms(2);
    lcd_byte(0x28, false); lcd_byte(0x0C, false);
    lcd_byte(0x06, false); lcd_byte(0x01, false);
}
static void redraw(void)
{
    uint8_t r, c;
    for (r = 0; r < 2U && healthy; ++r) {
        lcd_byte(r ? 0xC0 : 0x80, false);
        for (c = 0; c < 16U && healthy; ++c) lcd_byte((uint8_t)rows[r][c], true);
    }
}
void board_lcd_set(const char *top, const char *bottom)
{
    uint8_t r, c;
    const char *text[2]; text[0] = top; text[1] = bottom;
    for (r = 0; r < 2U; ++r) {
        bool end = false;
        for (c = 0; c < 16U; ++c) {
            if (!end && text[r][c] == 0) end = true;
            rows[r][c] = end ? ' ' : text[r][c];
        }
        rows[r][16] = 0;
    }
    redraw();
}
bool board_lcd_select(uint8_t next_mode, uint8_t next_address)
{
    uint8_t previous_mode = mode, previous_address = address;
    uint16_t previous_speed = i2c_bus_khz();
    if (next_mode > LCD_I2C || (next_mode == LCD_I2C &&
        !((next_address >= 0x20U && next_address <= 0x27U) ||
          (next_address >= 0x38U && next_address <= 0x3FU)))) return false;

    mode = next_mode; address = next_address; healthy = true;
    if (mode == LCD_I2C) {
        TRISB |= 0xFC00U;
        (void)i2c_bus_speed(100); /* PCF8574 Standard-mode bus */
        healthy = i2c_bus_recover();
    } else { LATB &= 0x03FFU; TRISB &= 0x03FFU; }
    initialize();
    redraw();
    if (healthy) return true;
    /* Failed new selection: restore previous transport; report failure. */
    mode = previous_mode; address = previous_address;
    healthy = true;
    (void)i2c_bus_speed(previous_speed);
    if (mode == LCD_PARALLEL) { LATB &= 0x03FFU; TRISB &= 0x03FFU; }
    else TRISB |= 0xFC00U;
    initialize(); redraw();
    return false;
}
void board_lcd_init(void)
{
    memset(rows, ' ', sizeof rows); rows[0][16] = rows[1][16] = 0;
    (void)board_lcd_select(ACCESS_LCD_DEFAULT_MODE, ACCESS_LCD_I2C_ADDRESS);
}
uint8_t board_lcd_mode(void) { return mode; }
uint8_t board_lcd_address(void) { return address; }
bool board_lcd_healthy(void) { return healthy; }
