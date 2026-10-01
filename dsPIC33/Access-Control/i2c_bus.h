#ifndef I2C_BUS_H
#define I2C_BUS_H
#include <stdint.h>
#include <stdbool.h>
void i2c_bus_init(void);
/* Single-master bus clear; bounded, does not replay a failed transaction. */
bool i2c_bus_recover(void);
bool i2c_bus_speed(uint16_t khz);
uint16_t i2c_bus_khz(void);
bool i2c_bus_transfer(uint8_t address, const uint8_t *tx, uint8_t wn, uint8_t *rx, uint8_t rn);
#endif
