#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdint.h>

void protocol_init(void);
void protocol_rx(uint8_t ch, uint32_t now);

#endif
