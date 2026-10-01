#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "access_config.h"
#include "access.h"
#include "board.h"

static char entry[9];
static uint8_t entry_len;
static uint8_t failures;
static bool relay_on, session_on, lockout_on, buzzer_on;
static uint32_t relay_until, session_until, lockout_until, buzzer_until;

static bool before(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) < 0;
}

static void beep(uint32_t now, uint16_t duration)
{
    board_buzzer_set(true);
    buzzer_on = true;
    buzzer_until = now + duration;
}

static void clear_entry(void)
{
    memset(entry, 0, sizeof entry);
    entry_len = 0;
}

static void show_entry(void)
{
    char masked[17] = "PIN: ";
    uint8_t i;
    for (i = 0; i < entry_len; ++i) masked[5U + i] = '*';
    masked[5U + entry_len] = 0;
    board_lcd_set("TK LINK ACCESS", masked);
}

static bool pin_equals(const char *pin)
{
    const char *expected = ACCESS_PIN;
    uint8_t i;
    uint8_t diff = 0;
    if (strlen(pin) != strlen(expected)) return false;
    for (i = 0; expected[i] != 0; ++i) diff |= (uint8_t)(pin[i] ^ expected[i]);
    return diff == 0U;
}

bool access_locked_out(uint32_t now)
{
    return lockout_on && before(now, lockout_until);
}

bool access_relay_is_on(void) { return relay_on; }
bool access_session_is_on(uint32_t now)
{
    return session_on && before(now, session_until);
}

void access_init(uint32_t now)
{
    (void)now;
    board_relay_set(false);
    board_buzzer_set(false);
    clear_entry();
    failures = 0;
    relay_on = session_on = lockout_on = buzzer_on = false;
    board_lcd_set("TK LINK ACCESS", "Ingrese PIN #");
}

void access_tick(uint32_t now)
{
    if (relay_on && !before(now, relay_until)) {
        board_relay_set(false);
        relay_on = false;
        board_lcd_set("TK LINK ACCESS", "Ingrese PIN #");
    }
    if (session_on && !before(now, session_until)) session_on = false;
    if (buzzer_on && !before(now, buzzer_until)) {
        board_buzzer_set(false);
        buzzer_on = false;
    }
    if (lockout_on && !before(now, lockout_until)) {
        lockout_on = false;
        board_lcd_set("TK LINK ACCESS", "Ingrese PIN #");
    }
}

static void open_relay(uint32_t now)
{
    clear_entry();
    if (!relay_on) {
        board_relay_set(true);
        relay_on = true;
        relay_until = now + ACCESS_OPEN_MS;
    }
    beep(now, 100);
    board_lcd_set("Acceso permitido", "Apertura activa");
}

static void failed(uint32_t now)
{
    clear_entry();
    session_on = false;
    if (++failures >= ACCESS_MAX_FAILURES) {
        failures = 0;
        lockout_on = true;
        lockout_until = now + ACCESS_LOCKOUT_MS;
        board_lcd_set("Acceso bloqueado", "Espere 30 s");
    } else board_lcd_set("PIN incorrecto", "Intente de nuevo");
    beep(now, 350);
}

bool access_authenticate(const char *pin, uint32_t now)
{
    if (access_locked_out(now)) return false;
    if (!pin_equals(pin)) {
        failed(now);
        return false;
    }
    failures = 0;
    session_on = true;
    session_until = now + ACCESS_SESSION_MS;
    return true;
}

bool access_remote_open(uint32_t now)
{
    if (access_locked_out(now) || !access_session_is_on(now)) return false;
    session_on = false;      /* una apertura por autenticación */
    open_relay(now);
    return true;
}

void access_logout(void) { session_on = false; }

void access_cancel(uint32_t now)
{
    (void)now;
    clear_entry();
    if (!lockout_on && !relay_on)
        board_lcd_set("TK LINK ACCESS", "Ingrese PIN #");
}

void access_show_status(uint32_t now)
{
    if (access_locked_out(now)) board_lcd_set("Acceso bloqueado", "Espere 30 s");
    else if (relay_on) board_lcd_set("Acceso permitido", "Apertura activa");
    else show_entry();
}

void access_key(char key, uint32_t now)
{
    if (access_locked_out(now) || relay_on) return;
    if (key >= '0' && key <= '9') {
        if (entry_len < 8U) {
            entry[entry_len++] = key;
            entry[entry_len] = 0;
            show_entry();
            beep(now, 35);
        }
    } else if (key == '*') {
        if (entry_len) entry[--entry_len] = 0;
        show_entry();
    } else if (key == 'A') access_cancel(now);
    else if (key == '#') {
        if (entry_len < 4U) { clear_entry(); show_entry(); return; }
        if (access_authenticate(entry, now)) {
            session_on = false;  /* PIN local no crea sesión remota */
            open_relay(now);
        }
        clear_entry();
    }
}
