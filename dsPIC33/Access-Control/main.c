#include <xc.h>
#include "access_config.h"
#include "board.h"
#include "access.h"
#include "protocol.h"
#include "aux.h"

#if defined(__dsPIC33FJ32MC204__)
#pragma config BWRP = WRPROTECT_OFF, BSS = NO_FLASH, GWRP = OFF, GSS = OFF
#pragma config FNOSC = PRIPLL, IESO = OFF, POSCMD = XT
#pragma config OSCIOFNC = OFF, IOL1WAY = OFF, FCKSM = CSDCMD
#pragma config WDTPOST = PS32768, WDTPRE = PR128, WINDIS = OFF, FWDTEN = OFF
#pragma config FPWRT = PWR128, ALTI2C = OFF, LPOL = ON, HPOL = ON, PWMPIN = ON
#pragma config ICS = PGD1, JTAGEN = OFF
#elif defined(__dsPIC33EP32MC204__)
#pragma config ICS = PGD3, JTAGEN = OFF
#pragma config ALTI2C1 = ON, ALTI2C2 = OFF
#pragma config PLLKEN = ON, FWDTEN = OFF
#pragma config POSCMD = HS, FNOSC = PRIPLL, IESO = OFF
#pragma config OSCIOFNC = OFF, IOL1WAY = OFF, FCKSM = CSDCMD
#else
#error "Selecciona dsPIC33FJ32MC204 o dsPIC33EP32MC204 en MPLAB X."
#endif

int main(void)
{
    uint32_t now;
    uint8_t ch, budget;
    char key;
    PLLFBD = 38;             /* 8 MHz * 40 / (2*2) = 80 MHz FOSC */
    CLKDIVbits.PLLPOST = 0;
    CLKDIVbits.PLLPRE = 0;
    while (!OSCCONbits.LOCK) { }  /* inicializar UART/timer solo con PLL estable */
    board_init();
    now = board_millis();
    access_init(now);
    protocol_init();
    aux_init();
    for (;;) {
        now = board_millis();
        access_tick(now);
        aux_tick(now);
        key = board_keypad_poll(now);
        if (key != 0) access_key(key, now);
        if (board_button1_pressed(now)) access_cancel(now);
        if (board_button2_pressed(now)) access_show_status(now);
        for (budget=0; budget<16U && board_uart_get(&ch); ++budget)
            protocol_rx(ch, board_millis());
    }
}
