#include <string.h>
#include <stdio.h>
#include "aux.h"
#include "aux_hal.h"
#include "aux_nvm.h"
#include "board.h"
#include "i2c_bus.h"
#include <xc.h>

static aux_config_t config[AUX_PINS];
static const char * const names[5]={"RA0","RA1","RA4","RB5","RB6"};
static const char * const modes[7]={"OFF","DI","DO","ADC","PWM","COUNT","FREQ"};
static struct {
    volatile uint32_t edges;
    uint32_t last, previous, hz;
    volatile uint16_t value, candidate, age;
    uint16_t published;
    bool valid;
} state[5];
static struct {
    uint32_t sum, window_sum;
    int32_t iir;
    uint16_t window[8];
    uint8_t samples, cursor, filled;
    volatile bool ready;
} adc[2];
static bool loaded, dirty;
extern void aux_hal_budget_tick(void);
static bool number(const char *s,uint32_t *v)
{
    uint32_t n=0; uint8_t i=0;
    if(!*s) return false;
    while(*s) {
        uint8_t d=(uint8_t)(*s++-'0');
        if(d>9 || ++i>10 || n>(0xFFFFFFFFUL-d)/10UL) return false;
        n=n*10UL+d;
    }
    *v=n; return true;
}
static int pin_id(const char *s)
{
    uint8_t i; for(i=0;i<5;++i) if(strcmp(s,names[i])==0) return i;
    return -1;
}
static int mode_id(const char *s)
{
    uint8_t i; for(i=0;i<7;++i) if(strcmp(s,modes[i])==0) return i;
    return -1;
}
static uint8_t split(char *s,char **parts,uint8_t max)
{
    uint8_t n=0;
    for(;;) {
        char *c;
        if(n==max) return 0;
        parts[n++]=s; c=strchr(s,','); if(!c) return n;
        *c=0; s=c+1;
    }
}
void aux_digital_tick(uint32_t now)
{
    uint8_t p; (void)now; aux_hal_budget_tick();
    for(p=0;p<5;++p) if(config[p].mode==AUX_DI) {
        uint16_t raw=aux_hal_level(p) ? 1 : 0;
        if(raw!=state[p].candidate) { state[p].candidate=raw; state[p].age=0; }
        if(state[p].age<1001) ++state[p].age;
        if(state[p].value!=raw && state[p].age>=config[p].p[2]) {
            state[p].value=raw;
            if(config[p].p[1]==3 || (raw && config[p].p[1]==1) || (!raw && config[p].p[1]==2)) ++state[p].edges;
        }
    }
}
void aux_adc_sample(uint8_t p,uint16_t value)
{
    uint16_t avg; uint32_t filter=config[p].p[4];
    if(config[p].mode!=AUX_ADC) return;
    adc[p].sum+=value;
    if(++adc[p].samples<config[p].p[3]) return;
    avg=(uint16_t)((adc[p].sum+adc[p].samples/2U)/adc[p].samples);
    adc[p].sum=0; adc[p].samples=0;
    if(filter==1) {
        adc[p].window_sum-=adc[p].window[adc[p].cursor];
        adc[p].window[adc[p].cursor]=avg; adc[p].window_sum+=avg;
        adc[p].cursor=(adc[p].cursor+1U)&7U;
        if(adc[p].filled<8) ++adc[p].filled;
        avg=(uint16_t)(adc[p].window_sum/adc[p].filled);
    } else if(filter==2) {
        if(!adc[p].filled) { adc[p].iir=(int32_t)avg*256L; adc[p].filled=1; }
        else adc[p].iir+=((int32_t)avg*256L-adc[p].iir)/8L;
        avg=(uint16_t)((adc[p].iir+128L)/256L);
    }
    state[p].value=avg; adc[p].ready=true;
}
void aux_tick(uint32_t now)
{
    uint8_t p;
    for(p=0;p<5;++p) {
        uint32_t interval=config[p].mode==AUX_ADC ? config[p].p[5] : config[p].p[0];
        if(config[p].mode==AUX_ADC) {
            if((uint32_t)(now-state[p].last)>=interval) { state[p].published=state[p].value; state[p].valid=adc[p].ready; state[p].last=now; }
        } else if(config[p].mode==AUX_COUNT || config[p].mode==AUX_FREQ) {
            uint32_t dt=now-state[p].last;
            if(dt>=interval) {
                uint32_t count; bool fault;
                aux_hal_snapshot(p,&count,&fault);
                /* Rate in Hz, no serial streaming load on the sample/capture ISRs. */
                state[p].hz=(uint32_t)(((uint64_t)(count-state[p].previous)*1000UL)/dt);
                state[p].previous=count; state[p].last=now;
            }
        }
    }
}
void aux_init(void)
{
    uint8_t p; uint16_t speed=100;
    memset(config,0,sizeof config); memset(state,0,sizeof state); memset(adc,0,sizeof adc);
    loaded=aux_nvm_load(config,&speed);
    for(p=0;p<5;++p) if(aux_validate(p,&config[p],config)) { loaded=false; break; }
    if(!loaded) { memset(config,0,sizeof config); speed=100; }
    if(board_lcd_mode()==LCD_I2C) speed=100;
    (void)i2c_bus_speed(speed);
    aux_hal_init(); aux_hal_resume(config); dirty=false;
}
static void describe(uint8_t p,char *out)
{
    const aux_config_t *c=&config[p];
    (void)snprintf(out,AUX_REPLY,"PIN=%s;MODE=%s;A=%lu;B=%lu;C=%lu;D=%lu;E=%lu;F=%lu",
        names[p],modes[c->mode],(unsigned long)c->p[0],(unsigned long)c->p[1],(unsigned long)c->p[2],
        (unsigned long)c->p[3],(unsigned long)c->p[4],(unsigned long)c->p[5]);
}
static int hx(char c) { if(c>='0' && c<='9') return c-'0'; if(c>='A' && c<='F') return c-'A'+10; return -1; }

bool aux_command(const char *verb,char *payload,char *out,bool *ok,uint32_t now)
{
    char *parts[8]; uint8_t n,i; int p; uint32_t value;
    *ok=false; strcpy(out,"BAD_ARGUMENT");
    if(strcmp(verb,"AUXCAP")==0) {
        if(!*payload) {
            strcpy(out,"AUX=1;PINS=RA0,RA1,RA4,RB5,RB6;ADC_MAX=1000;PWM_MIN=10;PWM_MAX=100000;NVM=1;POLL_MIN=500"); *ok=true;
        } else if((p=pin_id(payload))>=0) {
            (void)snprintf(out,AUX_REPLY,"PIN=%s;MODES=%s",names[p],p<2 ? "OFF,DI,DO,ADC" : p==2 ? "OFF,DI,DO,COUNT,FREQ" : "OFF,DI,DO,PWM,COUNT,FREQ"); *ok=true;
        }
    } else if(strcmp(verb,"AUXGET")==0) {
        if((p=pin_id(payload))>=0) { describe((uint8_t)p,out); *ok=true; }
    } else if(strcmp(verb,"AUXSET")==0) {
        aux_config_t next; const char *error; uint16_t sr;
        memset(&next,0,sizeof next); n=split(payload,parts,8);
        if(n<2 || (p=pin_id(parts[0]))<0) return true;
        { int m=mode_id(parts[1]); if(m<0) return true; next.mode=(uint16_t)m; }
        { static const uint8_t args[]={0,3,1,6,4,1,1}; if(n!=args[next.mode]+2U) return true; }
        for(i=2;i<n;++i) if(!number(parts[i],&next.p[i-2])) return true;
        error=aux_validate((uint8_t)p,&next,config);
        if(error) { strcpy(out,error); return true; }
        sr=SR; SRbits.IPL=7; config[p]=next;
        memset(&state[p],0,sizeof state[p]); state[p].last=now;
        if(p<2) memset(&adc[p],0,sizeof adc[p]);
        aux_hal_apply((uint8_t)p,config);
        if(next.mode==AUX_DI) state[p].candidate=state[p].value=aux_hal_level((uint8_t)p) ? 1 : 0;
        SR=sr; dirty=true; describe((uint8_t)p,out); *ok=true;
    } else if(strcmp(verb,"AUXREAD")==0) {
        uint32_t count=0,hz=0; uint16_t v=0,sr; bool fault=false,valid=true;
        if((p=pin_id(payload))<0) return true;
        sr=SR; SRbits.IPL=7;
        if(config[p].mode==AUX_DI) { v=state[p].value; count=state[p].edges; }
        else if(config[p].mode==AUX_ADC) { v=state[p].published; valid=state[p].valid; }
        else if(config[p].mode==AUX_DO) v=(uint16_t)config[p].p[0];
        else if(config[p].mode==AUX_COUNT || config[p].mode==AUX_FREQ) { aux_hal_snapshot((uint8_t)p,&count,&fault); hz=state[p].hz; }
        else if(config[p].mode==AUX_PWM) { aux_timer_t t; (void)aux_timer_plan(config[p].p[0],&t); hz=t.actual_hz; v=(uint16_t)config[p].p[1]; }
        SR=sr;
        (void)snprintf(out,AUX_REPLY,"PIN=%s;MODE=%s;VALUE=%u;COUNT=%lu;HZ=%lu;FAULT=%u;VALID=%u",names[p],modes[config[p].mode],v,(unsigned long)count,(unsigned long)hz,fault ? 1U : 0U,valid ? 1U : 0U); *ok=true;
    } else if(strcmp(verb,"AUXSAVE")==0 && !*payload) {
        if(!dirty) { strcpy(out,"SAVED=1;CHANGED=0"); *ok=true; return true; }
        aux_hal_pause();
        *ok=aux_nvm_save(config,i2c_bus_khz());
        memset(state,0,sizeof state); memset(adc,0,sizeof adc);
        for(i=0;i<AUX_PINS;++i) state[i].last=board_millis();
        aux_hal_resume(config);
        if(*ok) { dirty=false; loaded=true; strcpy(out,"SAVED=1;COUNTERS=RESET"); }
        else strcpy(out,"NVM_ERROR");
    } else if(strcmp(verb,"AUXNVM")==0 && !*payload) {
        (void)snprintf(out,AUX_REPLY,"LOADED=%u;DIRTY=%u;SCHEMA=1",loaded ? 1U : 0U,dirty ? 1U : 0U); *ok=true;
    } else if(strcmp(verb,"I2CGET")==0 && !*payload) {
        (void)snprintf(out,AUX_REPLY,"KHZ=%u;MAX=%u;LCD=%u",i2c_bus_khz(),board_lcd_mode()==LCD_I2C ? 100U : 1000U,board_lcd_mode()==LCD_I2C ? board_lcd_address() : 0U); *ok=true;
    } else if(strcmp(verb,"I2CSET")==0) {
        if(!number(payload,&value) || (value!=100 && value!=400 && value!=1000)) return true;
        if(board_lcd_mode()==LCD_I2C && value!=100) { strcpy(out,"I2C_LCD_100K"); return true; }
        *ok=i2c_bus_speed((uint16_t)value); dirty=true;
        (void)snprintf(out,AUX_REPLY,"KHZ=%u",i2c_bus_khz());
    } else if(strcmp(verb,"I2CXFER")==0) {
        uint8_t address,tx[16],rx[16],wn=0,rn;
        n=split(payload,parts,3);
        if(n!=3 || strlen(parts[0])!=2 || hx(parts[0][0])<0 || hx(parts[0][1])<0 || !number(parts[2],&value) || value>16) return true;
        address=(uint8_t)((hx(parts[0][0])<<4)|hx(parts[0][1])); rn=(uint8_t)value;
        if(address<8 || address>0x77) { strcpy(out,"BAD_ADDRESS"); return true; }
        if(board_lcd_mode()==LCD_I2C && address==board_lcd_address()) { strcpy(out,"I2C_LCD_RESERVED"); return true; }
        if(strlen(parts[1])>32 || (strlen(parts[1])&1U)) return true;
        for(i=0;parts[1][i];i+=2) {
            if(hx(parts[1][i])<0 || hx(parts[1][i+1])<0) return true;
            tx[wn++]=(uint8_t)((hx(parts[1][i])<<4)|hx(parts[1][i+1]));
        }
        if(!i2c_bus_transfer(address,tx,wn,rx,rn)) { strcpy(out,"I2C_NACK_TIMEOUT"); return true; }
        strcpy(out,"DATA=");
        for(i=0;i<rn;++i) { static const char digits[]="0123456789ABCDEF"; out[5+2*i]=digits[rx[i]>>4]; out[6+2*i]=digits[rx[i]&15]; }
        out[5+2*rn]=0; *ok=true;
    } else return false;
    return true;
}
