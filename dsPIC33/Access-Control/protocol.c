#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "access_config.h"
#include "access.h"
#include "board.h"
#include "protocol.h"
#include "aux.h"

#define RX_MAX 96U
static char rx[RX_MAX];
static uint8_t used;
static bool drop_line;

static uint16_t crc16(const char *buf, uint16_t size)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    for (i = 0; i < size; ++i) {
        uint8_t bit;
        crc ^= (uint16_t)((uint16_t)(uint8_t)buf[i] << 8);
        for (bit = 0; bit < 8U; ++bit) {
            if ((crc & 0x8000U) != 0U)
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021U);
            else crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void out_char(char c) { board_uart_put(c); }

static void reply(const char *seq, const char *kind, const char *payload)
{
    char body[180];
    uint16_t crc;
    uint16_t i, length;
    const char digits[] = "0123456789ABCDEF";
    body[0] = 0;
    /* Las cadenas están acotadas por construcción; no entra texto RX aquí
       salvo una secuencia decimal validada de hasta tres caracteres. */
    strcat(body, "TK1|");
    strcat(body, seq);
    strcat(body, "|");
    strcat(body, kind);
    strcat(body, "|");
    strcat(body, payload);
    length = (uint16_t)strlen(body);
    crc = crc16(body, length);
    out_char('@');
    for (i = 0; i < length; ++i) out_char(body[i]);
    out_char('|');
    out_char(digits[(crc >> 12) & 15U]);
    out_char(digits[(crc >> 8) & 15U]);
    out_char(digits[(crc >> 4) & 15U]);
    out_char(digits[crc & 15U]);
    out_char('\n');
}

static bool valid_pin_payload(const char *p)
{
    uint8_t len = 0;
    while (*p) {
        if (*p < '0' || *p > '9' || ++len > 8U) return false;
        ++p;
    }
    return len >= 4U;
}

static void lcd_status(const char *seq)
{
    char value[40];
    char addr[3];
    const char hex[] = "0123456789ABCDEF";
    uint8_t a = board_lcd_address();
    addr[0] = hex[a >> 4]; addr[1] = hex[a & 15U]; addr[2] = 0;
    strcpy(value, board_lcd_mode() == LCD_I2C ? "MODE=I2C;ADDR=" : "MODE=PARALLEL;ADDR=");
    strcat(value, addr);
    strcat(value, board_lcd_healthy() ? ";OK=1" : ";OK=0");
    reply(seq, "OK", value);
}

static void handle(uint32_t now)
{
    char *last, *first, *second, *third;
    char *seq, *verb, *payload;
    uint16_t crc = 0;
    uint8_t i;
    if (used < 15U || rx[0] != '@') return;
    last = strrchr(rx, '|');
    if (!last || strlen(last + 1) != 4U) return;
    for (i = 0; i < 4U; ++i) {
        int n = hexval(last[1U + i]);
        if (n < 0) return;
        crc = (uint16_t)((crc << 4) | (uint16_t)n);
    }
    if (crc != crc16(rx + 1, (uint16_t)(last - rx - 1))) return;
    *last = 0;
    first = strchr(rx + 1, '|');
    if (!first || (first - (rx + 1)) != 3 || strncmp(rx + 1, "TK1", 3) != 0)
        return;
    seq = first + 1;
    second = strchr(seq, '|');
    if (!second || second == seq || (second - seq) > 3) return;
    *second = 0;
    for (i = 0; seq[i]; ++i) if (seq[i] < '0' || seq[i] > '9') return;
    verb = second + 1;
    third = strchr(verb, '|');
    if (!third || third == verb) return;
    *third = 0;
    payload = third + 1;
    if (strchr(payload, '|')) { reply(seq, "ERR", "BAD_FORMAT"); return; }

    if (strcmp(verb, "HELLO") == 0 && payload[0] == 0)
        reply(seq, "OK", "MODEL=" ACCESS_MODEL ";HW=" ACCESS_HW_REV
              ";FW=" ACCESS_FW_VERSION
              ";CAP=ACCESS,KEYPAD,LCD,LCD_MODE,BUZZER,BUTTONS,AUX1;PROTO=1");
    else if (strcmp(verb, "STATUS") == 0 && payload[0] == 0) {
        if (access_locked_out(now)) reply(seq, "OK", "STATE=LOCKOUT");
        else if (access_relay_is_on()) reply(seq, "OK", "STATE=OPEN");
        else reply(seq, "OK", "STATE=CLOSED");
    } else if (strcmp(verb, "AUTH") == 0 && valid_pin_payload(payload)) {
        if (access_locked_out(now)) reply(seq, "ERR", "LOCKOUT");
        else if (access_authenticate(payload, now)) reply(seq, "OK", "AUTH=1");
        else reply(seq, "ERR", "BAD_PIN");
        memset(payload, 0, strlen(payload));
    } else if (strcmp(verb, "OPEN") == 0 && payload[0] == 0) {
        if (access_locked_out(now)) reply(seq, "ERR", "LOCKOUT");
        else if (access_remote_open(now)) reply(seq, "OK", "STATE=OPEN");
        else reply(seq, "ERR", "AUTH_REQUIRED");
    } else if (strcmp(verb, "LCDGET") == 0 && payload[0] == 0) {
        lcd_status(seq);
    } else if (strcmp(verb, "LCDSET") == 0) {
        uint8_t mode = LCD_PARALLEL, addr = board_lcd_address();
        if (!access_session_is_on(now) || access_locked_out(now)) {
            reply(seq, "ERR", "AUTH_REQUIRED"); return;
        }
        access_logout();
        if (access_relay_is_on()) { reply(seq, "ERR", "BUSY"); return; }
        if (strcmp(payload, "PARALLEL") == 0) mode = LCD_PARALLEL;
        else if (strlen(payload) == 6U && strncmp(payload, "I2C,", 4) == 0 &&
                 hexval(payload[4]) >= 0 && hexval(payload[5]) >= 0) {
            mode = LCD_I2C;
            addr = (uint8_t)((hexval(payload[4]) << 4) | hexval(payload[5]));
            if (!((addr >= 0x20U && addr <= 0x27U) || (addr >= 0x38U && addr <= 0x3FU))) {
                reply(seq, "ERR", "BAD_ADDRESS"); return;
            }
        } else { reply(seq, "ERR", "BAD_ARGUMENT"); return; }
        if (!board_lcd_select(mode, addr)) { reply(seq, "ERR", "LCD_I2C_ERROR"); return; }
        lcd_status(seq);
    } else if (strncmp(verb, "AUX", 3) == 0 || strncmp(verb, "I2C", 3) == 0) {
        char response[AUX_REPLY]; bool ok;
        bool write = strcmp(verb,"AUXSET")==0 || strcmp(verb,"AUXSAVE")==0 ||
                     strcmp(verb,"I2CSET")==0 || strcmp(verb,"I2CXFER")==0;
        if(write) {
            if(!access_session_is_on(now) || access_locked_out(now)) { reply(seq,"ERR","AUTH_REQUIRED"); return; }
            access_logout();
            if(access_relay_is_on()) { reply(seq,"ERR","BUSY"); return; }
        }
        if(aux_command(verb,payload,response,&ok,now)) reply(seq,ok ? "OK" : "ERR",response);
        else reply(seq,"ERR","UNKNOWN_COMMAND");
    } else if (strcmp(verb, "BYE") == 0 && payload[0] == 0) {
        access_logout();
        reply(seq, "OK", "SESSION=ENDED");
    } else reply(seq, "ERR", "UNKNOWN_COMMAND");
}

void protocol_init(void)
{
    used = 0;
    drop_line = false;
    memset(rx, 0, sizeof rx);
}

void protocol_rx(uint8_t ch, uint32_t now)
{
    if (ch == '\r') return;
    if (ch == '\n') {
        if (!drop_line && used) { rx[used] = 0; handle(now); }
        memset(rx, 0, sizeof rx);
        used = 0;
        drop_line = false;
    } else if (ch < 32U || ch > 126U || used >= RX_MAX - 1U)
        drop_line = true;
    else if (!drop_line) rx[used++] = (char)ch;
}
