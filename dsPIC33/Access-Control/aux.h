#ifndef AUX_H
#define AUX_H
#include <stdint.h>
#include <stdbool.h>
#define AUX_PINS 5U
#define AUX_REPLY 144U
#define AUX_SCHEMA 1U
enum { AUX_OFF, AUX_DI, AUX_DO, AUX_ADC, AUX_PWM, AUX_COUNT, AUX_FREQ };
/* Physical parameters; layout and units are documented in AUX_PROTOCOL.md. */
typedef struct { uint16_t mode; uint32_t p[6]; } aux_config_t;
typedef struct { uint16_t prescale, period; uint32_t actual_hz; } aux_timer_t;
const char *aux_validate(uint8_t pin, const aux_config_t *next, const aux_config_t *all);
bool aux_timer_plan(uint32_t hz, aux_timer_t *plan);
void aux_init(void);
void aux_tick(uint32_t now);
/* Called by ADC ISR. No allocation, serial, LCD or floating point in ISR. */
void aux_adc_sample(uint8_t pin, uint16_t value);
void aux_digital_tick(uint32_t now);
bool aux_command(const char *verb, char *payload, char *reply, bool *ok, uint32_t now);
#endif
