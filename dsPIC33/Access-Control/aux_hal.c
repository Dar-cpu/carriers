#include <xc.h>
#include "access_config.h"
#include "board.h"
#include "aux_hal.h"

static volatile uint32_t pulses[2], t1_high;
static volatile uint16_t burst[2];
static volatile bool fault[2];
static volatile uint8_t adc_pin, adc_slot;
static uint16_t phase[2], adc_div[2], adc_sam[2];
static bool adc_running;
static const aux_config_t *config;
static uint16_t pinmask(uint8_t p) { return (uint16_t)(1U << (p<2 ? p : p+2)); }
static void gpio(uint8_t p,bool output,bool value,bool pull)
{
    uint16_t m=pinmask(p);
    if(p<3) {
        TRISA|=m; if(value) LATA|=m; else LATA&=(uint16_t)~m;
        if(output) TRISA&=(uint16_t)~m;
    } else {
        TRISB|=m; if(value) LATB|=m; else LATB&=(uint16_t)~m;
        if(output) TRISB&=(uint16_t)~m;
    }
#if defined(__dsPIC33FJ32MC204__)
    if(p<3) {
        uint16_t cn=p==0 ? 4U : p==1 ? 8U : 1U;
        if(pull) CNPU1|=cn; else CNPU1&=(uint16_t)~cn;
    } else {
        uint16_t cn=p==3 ? (1U<<11) : (1U<<8);
        if(pull) CNPU2|=cn; else CNPU2&=(uint16_t)~cn;
    }
    if(p<2) AD1PCFGL|=m;
#else
    if(p<3) { if(pull) CNPUA|=m; else CNPUA&=(uint16_t)~m; ANSELA&=(uint16_t)~m; }
    else { if(pull) CNPUB|=m; else CNPUB&=(uint16_t)~m; ANSELB&=(uint16_t)~m; }
#endif
}
bool aux_hal_level(uint8_t p) { return ((p<3 ? PORTA : PORTB)&pinmask(p))!=0; }
static void pps(uint8_t p,uint16_t function)
{
    __builtin_write_OSCCONL(OSCCON & ~(1U<<6));
#if defined(__dsPIC33FJ32MC204__)
    if(p==3) RPOR2bits.RP5R=function; else RPOR3bits.RP6R=function;
#else
    if(p==3) RPOR1bits.RP37R=function; else RPOR2bits.RP38R=function;
#endif
    __builtin_write_OSCCONL(OSCCON | (1U<<6));
}
static void capture_stop(uint8_t p)
{
    if(p==3) {
        IEC0bits.IC1IE=0;
#if defined(__dsPIC33FJ32MC204__)
        IC1CON=0;
#else
        IC1CON1=0; IC1CON2=0;
#endif
        IFS0bits.IC1IF=0;
    } else {
        IEC0bits.IC2IE=0;
#if defined(__dsPIC33FJ32MC204__)
        IC2CON=0;
#else
        IC2CON1=0; IC2CON2=0;
#endif
        IFS0bits.IC2IF=0;
    }
}
static void capture_start(uint8_t p)
{
    __builtin_write_OSCCONL(OSCCON & ~(1U<<6));
#if defined(__dsPIC33FJ32MC204__)
    if(p==3) RPINR7bits.IC1R=5; else RPINR7bits.IC2R=6;
#else
    if(p==3) RPINR7bits.IC1R=37; else RPINR7bits.IC2R=38;
#endif
    __builtin_write_OSCCONL(OSCCON | (1U<<6));
    if(p==3) {
#if defined(__dsPIC33FJ32MC204__)
        IC1CON=0x0083; /* Timer2; every rising edge */
#else
        IC1CON2=0; IC1CON1=0x0403; /* T2 clock, independent free-running counter */
#endif
        IPC0bits.IC1IP=3; IEC0bits.IC1IE=1;
    } else {
#if defined(__dsPIC33FJ32MC204__)
        IC2CON=0x0083;
#else
        IC2CON2=0; IC2CON1=0x0403;
#endif
        IPC1bits.IC2IP=3; IEC0bits.IC2IE=1;
    }
}
void __attribute__((interrupt,no_auto_psv)) _IC1Interrupt(void)
{
    uint8_t n=0; volatile uint16_t discard;
    IFS0bits.IC1IF=0;
#if defined(__dsPIC33FJ32MC204__)
#define IC1_STATUS IC1CONbits
#else
#define IC1_STATUS IC1CON1bits
#endif
    if(IC1_STATUS.ICOV) fault[0]=true;
    while(IC1_STATUS.ICBNE && n++<8) { discard=IC1BUF; ++pulses[0]; ++burst[0]; }
    (void)discard;
    if(burst[0]>32 || fault[0]) { fault[0]=true; capture_stop(3); }
}
void __attribute__((interrupt,no_auto_psv)) _IC2Interrupt(void)
{
    uint8_t n=0; volatile uint16_t discard;
    IFS0bits.IC2IF=0;
#if defined(__dsPIC33FJ32MC204__)
#define IC2_STATUS IC2CONbits
#else
#define IC2_STATUS IC2CON1bits
#endif
    if(IC2_STATUS.ICOV) fault[1]=true;
    while(IC2_STATUS.ICBNE && n++<8) { discard=IC2BUF; ++pulses[1]; ++burst[1]; }
    (void)discard;
    if(burst[1]>32 || fault[1]) { fault[1]=true; capture_stop(4); }
}
void __attribute__((interrupt,no_auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF=0; t1_high+=65536UL;
}
void aux_hal_snapshot(uint8_t p,uint32_t *count,bool *overflow)
{
    uint16_t sr=SR; SRbits.IPL=7;
    if(p==2) {
        uint32_t high=t1_high; uint16_t lo=TMR1;
        if(IFS0bits.T1IF) { high+=65536UL; lo=TMR1; }
        *count=high+lo; *overflow=false;
    } else { *count=pulses[p-3]; *overflow=fault[p-3]; }
    SR=sr;
}
static void pwm_rebuild(const aux_config_t *all)
{
    uint8_t p; aux_timer_t plan; bool any=false;
    T3CONbits.TON=0;
#if defined(__dsPIC33FJ32MC204__)
    OC1CON=0; OC2CON=0;
#else
    OC1CON1=0; OC2CON1=0;
#endif
    for(p=3;p<5;++p) {
        const aux_config_t *c=&all[p]; uint32_t ticks,high;
        if(c->mode!=AUX_PWM) continue;
        any=true; (void)aux_timer_plan(c->p[0],&plan);
        T3CON=0; T3CONbits.TCKPS=plan.prescale; PR3=plan.period; TMR3=0;
        ticks=(uint32_t)plan.period+1UL;
        /* Duty is the active fraction. Active-low has a low pulse of that width.
           Channels are not phase-synchronized; no complementary/dead-time mode. */
        high=(ticks*(c->p[2] ? 1000UL-c->p[1] : c->p[1])+500UL)/1000UL;
        pps(p,0); gpio(p,true,c->p[3]!=0,false);
        if(high==0 || high>=ticks) { gpio(p,true,high!=0,false); continue; }
#if defined(__dsPIC33FJ32MC204__)
        if(p==3) { OC1R=(uint16_t)high; OC1RS=(uint16_t)high; OC1CON=0x000E; }
        else { OC2R=(uint16_t)high; OC2RS=(uint16_t)high; OC2CON=0x000E; }
        pps(p,p==3 ? 18 : 19);
#else
        if(p==3) { OC1CON2=31; OC1R=(uint16_t)high; OC1RS=plan.period; OC1TMR=0; OC1CON1=0x0406; }
        else { OC2CON2=31; OC2R=(uint16_t)high; OC2RS=plan.period; OC2TMR=0; OC2CON1=0x0406; }
        pps(p,p==3 ? 16 : 17);
#endif
    }
    if(any) T3CONbits.TON=1;
}
static void adc_rebuild(const aux_config_t *all)
{
    uint8_t p; bool enabled=false;
    IEC0bits.AD1IE=0; adc_running=false; AD1CON1bits.ADON=0;
    AD1CON1=0; AD1CON2=0; AD1CON3=0;
    AD1CON1bits.SSRC=7; /* auto-convert after SAMC; software starts sampling */
    for(p=0;p<2;++p) {
        phase[p]=0;
        if(all[p].mode!=AUX_ADC) continue;
        enabled=true; AD1CON1bits.AD12B=(all[p].p[0]==12);
        {
            uint32_t acquire=(all[p].p[2] ? all[p].p[2] : 2UL)*1000UL;
            uint32_t divider=(acquire+774UL)/775UL, tad;
            if(divider<5) divider=5;
            tad=divider*25UL;
            adc_div[p]=(uint16_t)(divider-1UL);
            adc_sam[p]=(uint16_t)((acquire+tad-1UL)/tad);
        }
#if defined(__dsPIC33FJ32MC204__)
        AD1PCFGL&=(uint16_t)~(1U<<p);
#else
        ANSELA|=(1U<<p);
#endif
    }
    adc_slot=0; adc_pin=255;
    if(!enabled) return;
    AD1CHS0=0; AD1CSSL=0;
    AD1CON3bits.ADCS=4; AD1CON3bits.SAMC=16;
    IFS0bits.AD1IF=0; IPC3bits.AD1IP=4; IEC0bits.AD1IE=1;
    AD1CON1bits.ADON=1;
    adc_running=true;
}
void aux_hal_timer_tick(void)
{
    uint8_t p=adc_slot;
    if(!adc_running) return;
    adc_slot^=1;
    if(!config || config[p].mode!=AUX_ADC || adc_pin!=255) return;
    phase[p]+=(uint16_t)config[p].p[1];
    if(phase[p]<1000) return;
    phase[p]-=1000;
    AD1CON3bits.ADCS=adc_div[p];
    AD1CON3bits.SAMC=adc_sam[p];
    AD1CHS0bits.CH0SA=p; adc_pin=p; AD1CON1bits.SAMP=1;
}
#if defined(__dsPIC33FJ32MC204__)
#define ADC_VECTOR _ADC1Interrupt
#else
#define ADC_VECTOR _AD1Interrupt
#endif
void __attribute__((interrupt,no_auto_psv)) ADC_VECTOR(void)
{
    uint16_t v=ADC1BUF0; uint8_t p=adc_pin;
    IFS0bits.AD1IF=0; adc_pin=255;
    if(p<2) aux_adc_sample(p,v);
}
void aux_hal_apply(uint8_t p,const aux_config_t *all)
{
    uint16_t sr=SR; SRbits.IPL=7; config=all;
    if(p>=3) { capture_stop(p); pps(p,0); pulses[p-3]=0; fault[p-3]=false; burst[p-3]=0; }
    if(p==2) { IEC0bits.T1IE=0; T1CON=0; TMR1=0; t1_high=0; }
    gpio(p,all[p].mode==AUX_DO,all[p].p[0]!=0,all[p].mode==AUX_DI && all[p].p[0]!=0);
    if(p<2) adc_rebuild(all);
    if(p>=3) pwm_rebuild(all);
    if(all[p].mode==AUX_COUNT || all[p].mode==AUX_FREQ) {
        if(p==2) {
            PR1=65535; IFS0bits.T1IF=0; IPC0bits.T1IP=3; IEC0bits.T1IE=1;
            T1CONbits.TSYNC=1; T1CONbits.TCS=1; T1CONbits.TON=1;
        } else capture_start(p);
    }
    SR=sr;
}
void aux_hal_init(void)
{
    /* Timer2 belongs to board.c: common 500 us scheduler on both MCUs. */
}
void aux_hal_pause(void)
{
    IEC0bits.AD1IE=0; adc_running=false; AD1CON1bits.ADON=0;
    capture_stop(3); capture_stop(4); IEC0bits.T1IE=0; T1CON=0; T3CON=0;
#if defined(__dsPIC33FJ32MC204__)
    OC1CON=0; OC2CON=0;
#else
    OC1CON1=0; OC2CON1=0;
#endif
    pps(3,0); pps(4,0);
    { uint8_t p; for(p=0;p<5;++p) gpio(p,false,false,false); }
}
void aux_hal_resume(const aux_config_t *all)
{
    uint8_t p; for(p=0;p<5;++p) aux_hal_apply(p,all);
}
/* Called at 1 kHz by AUX digital sampling. */
void aux_hal_budget_tick(void) { burst[0]=0; burst[1]=0; }
