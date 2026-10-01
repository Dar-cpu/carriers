#ifndef AUX_HAL_H
#define AUX_HAL_H
#include "aux.h"
void aux_hal_init(void);
void aux_hal_timer_tick(void);
void aux_hal_apply(uint8_t pin, const aux_config_t *all);
bool aux_hal_level(uint8_t pin);
void aux_hal_snapshot(uint8_t pin, uint32_t *count, bool *overflow);
void aux_hal_pause(void);
void aux_hal_resume(const aux_config_t *all);
#endif
