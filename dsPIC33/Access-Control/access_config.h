#ifndef ACCESS_CONFIG_H
#define ACCESS_CONFIG_H

#define FCY 40000000UL
#define ACCESS_FW_VERSION "0.3.1"
#define ACCESS_MODEL "TKLINK-ACCESS-01"
#define ACCESS_HW_REV "A"
#define ACCESS_PIN "2580" /* SOLO BANCO: cambiar antes de compilar */
#define ACCESS_OPEN_MS 5000UL
#define ACCESS_SESSION_MS 30000UL
#define ACCESS_LOCKOUT_MS 30000UL
#define ACCESS_MAX_FAILURES 3U
#define ACCESS_UART_BAUD 9600UL
#define ACCESS_LCD_DEFAULT_MODE 0U /* 0=paralelo, 1=I2C */
#define ACCESS_LCD_I2C_ADDRESS 0x27U /* direccion de 7 bits */
/* Rele RB7: salida baja al abrir; entrada/alta impedancia al cerrar. */

#endif
