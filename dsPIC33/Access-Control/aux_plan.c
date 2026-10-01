#include "aux.h"
#include "access_config.h"
#include <stddef.h>

bool aux_timer_plan(uint32_t hz, aux_timer_t *plan)
{
    static const uint16_t divs[4] = {1,8,64,256};
    uint8_t i;
    if (hz < 10UL || hz > 100000UL) return false;
    for (i = 0; i < 4; ++i) {
        uint32_t clock = FCY / divs[i];
        uint32_t ticks = (clock + hz / 2UL) / hz;
        if (ticks >= 2UL && ticks <= 65535UL) {
            plan->prescale = i; plan->period = (uint16_t)(ticks - 1UL);
            plan->actual_hz = clock / ticks; return true;
        }
    }
    return false;
}

const char *aux_validate(uint8_t pin, const aux_config_t *c, const aux_config_t *all)
{
    uint8_t i, count = 0;
    aux_timer_t timer;
    if (pin >= AUX_PINS || c->mode > AUX_FREQ) return "BAD_MODE";
    switch (c->mode) {
    case AUX_OFF: break;
    case AUX_DI:
        count=3;
        if(c->p[0]>1 || c->p[1]>3 || c->p[2]>1000) return "BAD_RANGE";
        break;
    case AUX_DO:
        count=1;
        if(c->p[0]>1) return "BAD_RANGE";
        break;
    case AUX_ADC:
        count=6;
        if(pin>1) return "BAD_MODE";
        if((c->p[0]!=10 && c->p[0]!=12) || c->p[1]<1 || c->p[1]>1000 ||
           c->p[2]>100 || (c->p[3]!=1 && c->p[3]!=4 && c->p[3]!=8 && c->p[3]!=16) ||
           c->p[4]>2 || c->p[5]<10 || c->p[5]>10000) return "BAD_RANGE";
        if(all[1U-pin].mode==AUX_ADC && all[1U-pin].p[0]!=c->p[0]) return "ADC_SHARED_BITS";
        break;
    case AUX_PWM:
        count=4;
        if(pin<3) return "BAD_MODE";
        if(!aux_timer_plan(c->p[0],&timer) || c->p[1]>1000 || c->p[2]>1 || c->p[3]>1) return "BAD_RANGE";
        i = pin==3 ? 4 : 3;
        if(all[i].mode==AUX_PWM && all[i].p[0]!=c->p[0]) return "PWM_SHARED_FREQ";
        break;
    case AUX_COUNT: case AUX_FREQ:
        count=1;
        if(pin<2) return "BAD_MODE";
        if(c->p[0]<100 || c->p[0]>10000) return "BAD_RANGE";
        break;
    default: return "BAD_MODE";
    }
    for(i=count;i<6;++i) if(c->p[i]!=0) return "BAD_ARGUMENT";
    return NULL;
}
