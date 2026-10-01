#ifndef AUX_NVM_H
#define AUX_NVM_H
#include "aux.h"
bool aux_nvm_load(aux_config_t *config, uint16_t *khz);
bool aux_nvm_save(const aux_config_t *config, uint16_t khz);
#endif
