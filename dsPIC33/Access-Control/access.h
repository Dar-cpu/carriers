#ifndef ACCESS_H
#define ACCESS_H
#include <stdint.h>
#include <stdbool.h>

void access_init(uint32_t now);
void access_tick(uint32_t now);
void access_key(char key, uint32_t now);
void access_cancel(uint32_t now);
void access_show_status(uint32_t now);
bool access_authenticate(const char *pin, uint32_t now);
bool access_remote_open(uint32_t now);
void access_logout(void);
bool access_locked_out(uint32_t now);
bool access_relay_is_on(void);
bool access_session_is_on(uint32_t now);

#endif
