/* Shared single-master I2C1, RB8=SCL / RB9=SDA; main-context only.
 * LCD and AUX use the same generic driver. No automatic write retries.
 */
#include <xc.h>
#include "access_config.h"
#include <libpic30.h>
#include "i2c_bus.h"

#define WAIT_STEPS 1000U /* 2 ms per hardware event, plus ISR service time */
static uint16_t speed = 100;

static bool fault(void)
{
    return I2C1STATbits.BCL || I2C1STATbits.IWCOL || I2C1STATbits.I2COV;
}
static bool wait_idle(void)
{
    uint16_t n;
    for(n=0;n<WAIT_STEPS;++n) {
        if(fault()) return false;
        if(!(I2C1CON & 0x1FU) && !I2C1STATbits.TBF &&
           !I2C1STATbits.TRSTAT) return true;
        __delay_us(2);
    }
    return false;
}
/* Clear MI2C1IF BEFORE starting each event. Waiting for completion avoids
 * reading a previous ACKSTAT while the new byte is only in the TX buffer.
 * Interrupt delivery stays disabled: the hardware flag is polled.
 */
static bool wait_event(void)
{
    uint16_t n;
    for(n=0;n<WAIT_STEPS;++n) {
        if(fault()) return false;
        if(IFS1bits.MI2C1IF) return wait_idle();
        __delay_us(2);
    }
    return false;
}
bool i2c_bus_speed(uint16_t khz)
{
    uint32_t cycles;
    if(khz!=100 && khz!=400 && khz!=1000) return false;
    IEC1bits.MI2C1IE=0;
    I2C1CONbits.I2CEN=0;
    TRISBbits.TRISB8=1; TRISBbits.TRISB9=1;
    /* Conservative BRG: bus clock never exceeds requested frequency. */
    cycles=(FCY+(uint32_t)khz*1000UL-1UL)/((uint32_t)khz*1000UL);
    I2C1BRG=(uint16_t)(cycles-1UL);
    I2C1CON=0; I2C1STAT=0;
    IFS1bits.MI2C1IF=0;
    I2C1CONbits.DISSLW=(khz==100);
    I2C1CONbits.I2CEN=1;
    speed=khz;
    return true;
}
static bool release_scl(void)
{
    uint16_t n;
    TRISBbits.TRISB8=1;
    for(n=0;n<WAIT_STEPS;++n) {
        if(PORTBbits.RB8) { __delay_us(5); return true; }
        __delay_us(2);
    }
    return false;
}
bool i2c_bus_recover(void)
{
    uint8_t i;
    bool ok=false;
    I2C1CONbits.I2CEN=0; /* GPIO recovery only with peripheral disabled. */
    TRISBbits.TRISB8=1; TRISBbits.TRISB9=1;
    LATBbits.LATB8=0; LATBbits.LATB9=0;
    if(!release_scl()) goto done;
    if(!PORTBbits.RB9) {
        /* Release a slave interrupted part-way through a byte. */
        for(i=0;i<9U && !PORTBbits.RB9;++i) {
            TRISBbits.TRISB8=0; __delay_us(5);
            if(!release_scl()) goto done;
        }
        /* STOP: SDA low, release SCL, then release SDA. */
        TRISBbits.TRISB8=0;
        TRISBbits.TRISB9=0; __delay_us(5);
        if(!release_scl()) goto done;
        TRISBbits.TRISB9=1; __delay_us(5);
    }
    ok=PORTBbits.RB8 && PORTBbits.RB9;
done:
    TRISBbits.TRISB8=1; TRISBbits.TRISB9=1;
    (void)i2c_bus_speed(speed);
    __delay_us(5);
    return ok;
}
void i2c_bus_init(void)
{
    (void)i2c_bus_speed(100);
    (void)i2c_bus_recover();
}
uint16_t i2c_bus_khz(void) { return speed; }
static bool write_byte(uint8_t value)
{
    IFS1bits.MI2C1IF=0;
    I2C1TRN=value;
    return wait_event() && !I2C1STATbits.ACKSTAT;
}
bool i2c_bus_transfer(uint8_t address,const uint8_t *tx,uint8_t wn,uint8_t *rx,uint8_t rn)
{
    uint8_t i;
    bool ok=false;
    if(address<0x08 || address>0x77 || wn>16 || rn>16 ||
       (wn && !tx) || (rn && !rx)) return false;
    if(!wait_idle()) goto fail;
    /* A held line before START may be left by a prior reset. */
    if((!PORTBbits.RB8 || !PORTBbits.RB9) && !i2c_bus_recover()) return false;
    IFS1bits.MI2C1IF=0;
    I2C1CONbits.SEN=1;
    if(!wait_event()) goto fail;
    if(wn || !rn) {
        if(!write_byte((uint8_t)(address<<1))) goto finish;
        for(i=0;i<wn;++i) if(!write_byte(tx[i])) goto finish;
    }
    if(rn) {
        if(wn) {
            IFS1bits.MI2C1IF=0;
            I2C1CONbits.RSEN=1;
            if(!wait_event()) goto fail;
        }
        if(!write_byte((uint8_t)((address<<1)|1))) goto finish;
        for(i=0;i<rn;++i) {
            IFS1bits.MI2C1IF=0;
            I2C1CONbits.RCEN=1;
            if(!wait_event() || !I2C1STATbits.RBF) goto fail;
            rx[i]=(uint8_t)I2C1RCV;
            I2C1CONbits.ACKDT=(i+1U==rn);
            IFS1bits.MI2C1IF=0;
            I2C1CONbits.ACKEN=1;
            if(!wait_event()) goto fail;
        }
    }
    ok=true;
finish:
    if(!wait_idle()) goto fail;
    IFS1bits.MI2C1IF=0;
    I2C1CONbits.PEN=1;
    if(!wait_event()) goto fail;
    __delay_us(5); /* Standard-mode bus-free time before the next START. */
    return ok;
fail:
    (void)i2c_bus_recover();
    return false; /* Caller sees failure; never replay a partial write. */
}
