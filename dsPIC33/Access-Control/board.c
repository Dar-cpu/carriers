#include <xc.h>
#include "access_config.h" /* FCY debe estar definido antes de libpic30.h */
#include <libpic30.h>
#include <stdint.h>
#include <stdbool.h>
#include "board.h"
#include "aux.h"
#include "aux_hal.h"
#include "i2c_bus.h"

static volatile uint32_t tick_ms, relay_deadline;
static volatile bool relay_guard;
static volatile uint8_t rx_data[128];
static volatile uint8_t rx_head, rx_tail;

void __attribute__((interrupt, no_auto_psv)) _T2Interrupt(void)
{
    IFS0bits.T2IF = 0;
    static uint8_t half;
    aux_hal_timer_tick();
    if(++half==2) {
        half=0; ++tick_ms;
        if(relay_guard && (int32_t)(tick_ms-relay_deadline)>=0) {
            LATBbits.LATB7=1; /* Open-drain: liberar la linea. */
            relay_guard=false;
        }
        aux_digital_tick(tick_ms);
    }
}

void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void)
{
    uint8_t next;
    IFS0bits.U1RXIF = 0;
    while (U1STAbits.URXDA) {
        uint8_t value = (uint8_t)U1RXREG;
        next = (uint8_t)((rx_head + 1U) & 127U);
        if (next != rx_tail) { rx_data[rx_head] = value; rx_head = next; }
    }
    if (U1STAbits.OERR) U1STAbits.OERR = 0;
}

void board_relay_set(bool on)
{
    uint16_t sr=SR; SRbits.IPL=7;
    if(on) relay_deadline=tick_ms+ACCESS_OPEN_MS;
    relay_guard=on;
    /* ODCB7=1: LAT=0 conduce a GND; LAT=1 libera la linea. */
    LATBbits.LATB7 = on ? 0 : 1;
    SR=sr;
}

void board_buzzer_set(bool on) { LATBbits.LATB2 = on ? 1 : 0; }

uint32_t board_millis(void)
{
    uint32_t value;
    IEC0bits.T2IE = 0;
    value = tick_ms;
    IEC0bits.T2IE = 1;
    return value;
}

bool board_uart_get(uint8_t *out)
{
    if (rx_tail == rx_head) return false;
    *out = rx_data[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1U) & 127U);
    return true;
}

void board_uart_put(char c)
{
    while (U1STAbits.UTXBF) { }
    U1TXREG = (uint8_t)c;
}

void board_init(void)
{
    /* Pines latched antes de configurar TRIS, incluido el relé. */
    TRISBbits.TRISB7 = 1;   /* Liberar antes de configurar open-drain. */
    LATBbits.LATB7 = 1;
    ODCBbits.ODCB7 = 1;
    LATBbits.LATB2 = 0;
    LATB &= 0x03FFU;         /* LCD RS/EN/D4..D7 a 0 */
    LATC &= 0xFFF0U;         /* Filas RC0..RC3 a 0 */
#if defined(__dsPIC33FJ32MC204__)
    AD1PCFGL = 0xFFFFU;
#elif defined(__dsPIC33EP32MC204__)
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;
#else
#error "Microcontrolador no compatible."
#endif
    TRISBbits.TRISB2 = 0;
    TRISBbits.TRISB3 = 1;    /* STATE */
    TRISBbits.TRISB4 = 1;    /* EN sin utilizar: no forzar modo AT */
    TRISBbits.TRISB7 = 0;    /* Salida open-drain, LAT=1: rele apagado. */
    TRISB &= 0x03FFU;        /* LCD RB10..RB15 salida */
    TRISC = (uint16_t)((TRISC & 0xFC00U) | 0x02FFU);
    /* RC0..3 sin seleccionar=alta impedancia; RC4..7 entradas,
       RC8 salida y RC9 entrada. */

    __builtin_write_OSCCONL(OSCCON & ~(1U << 6));
#if defined(__dsPIC33FJ32MC204__)
    RPINR18bits.U1RXR = 25;     /* RC9/RP25 */
    RPOR12bits.RP24R = 3;      /* RC8/RP24, U1TX */
#else
    RPINR18bits.U1RXR = 57;     /* RC9/RP57 */
    RPOR6bits.RP56R = 1;       /* RC8/RP56, U1TX */
#endif
    __builtin_write_OSCCONL(OSCCON | (1U << 6));
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.BRGH = 0;
    U1BRG = (uint16_t)((FCY + 8UL * ACCESS_UART_BAUD) /
                        (16UL * ACCESS_UART_BAUD) - 1UL);
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;

    T2CON = 0;
    TMR2 = 0;
    T2CONbits.TCKPS = 1;       /* 1:8 */
    PR2 = (uint16_t)(FCY / 8UL / 2000UL - 1UL);
    IFS0bits.T2IF = 0;
    IPC1bits.T2IP = 5;
    IEC0bits.T2IE = 1;
    T2CONbits.TON = 1;
    i2c_bus_init();
    board_lcd_init();
}

char board_keypad_poll(uint32_t now)
{
    static const char map[4][4] = {
        {'1','2','3','A'}, {'4','5','6','B'},
        {'7','8','9','C'}, {'*','0','#','D'}
    };
    static uint32_t last;
    static char candidate, stable;
    static uint8_t count;
    char raw = 0;
    uint8_t row, col, hits = 0;
    if ((uint32_t)(now - last) < 10UL) return 0;
    last = now;
    for (row = 0; row < 4U; ++row) {
        LATC = (uint16_t)((LATC & 0xFFF0U) | (1U << row));
        TRISC = (uint16_t)(TRISC & ~(1U << row));
        __delay_us(10);
        for (col = 0; col < 4U; ++col) {
            if ((PORTC & (1U << (col + 4U))) != 0U) {
                ++hits;
                raw = map[row][col];
            }
        }
        TRISC = (uint16_t)(TRISC | (1U << row));
    }
    LATC &= 0xFFF0U;
    if (hits != 1U) raw = 0;
    if (raw != candidate) { candidate = raw; count = 0; }
    else if (count < 3U) ++count;
    if (count >= 3U && raw != stable) {
        stable = raw;
        return raw;
    }
    return 0;
}

static bool button_edge(uint32_t now, uint16_t mask, uint8_t index)
{
    static uint32_t last[2];
    static uint8_t candidate[2] = {1,1}, stable[2] = {1,1};
    uint8_t raw;
    if ((uint32_t)(now - last[index]) < 20UL) return false;
    last[index] = now;
    raw = (PORTA & mask) ? 1U : 0U;
    if (raw == candidate[index] && stable[index] != raw) {
        stable[index] = raw;
        return raw == 0U;
    }
    candidate[index] = raw;
    return false;
}

bool board_button1_pressed(uint32_t now) { return button_edge(now, (1U<<7), 0); }
bool board_button2_pressed(uint32_t now) { return button_edge(now, (1U<<10), 1); }
