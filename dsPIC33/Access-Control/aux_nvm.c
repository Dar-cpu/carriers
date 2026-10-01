/* Two independent erase pages. The previous valid snapshot survives a failed
 * erase/program. Version + CRC protect boot; user explicitly requests saves.
 * This stores AUX physical configuration only, never credentials or counters. */
#include <xc.h>
#include <string.h>
#include "aux_nvm.h"
#if defined(__dsPIC33FJ32MC204__)
#define PAGE_WORDS 512
#define PAGE_ALIGN 1024
#else
#define PAGE_WORDS 1024
#define PAGE_ALIGN 2048
#endif
#define RECORD_WORDS 128U
static const uint16_t page_a[PAGE_WORDS] __attribute__((space(prog),aligned(PAGE_ALIGN),section(".aux_store_a")))={0xFFFF};
static const uint16_t page_b[PAGE_WORDS] __attribute__((space(prog),aligned(PAGE_ALIGN),section(".aux_store_b")))={0xFFFF};
static uint16_t record[RECORD_WORDS];
static uint32_t generation;
static uint8_t active=255;
static uint16_t checksum(void)
{
    uint16_t crc=0xFFFF,i;
    const uint8_t *p=(const uint8_t *)record;
    for(i=0;i<(RECORD_WORDS-1U)*2U;++i) {
        uint8_t b; crc^=(uint16_t)((uint16_t)p[i]<<8);
        for(b=0;b<8;++b) crc=(crc & 0x8000U) ? (uint16_t)((crc<<1)^0x1021U) : (uint16_t)(crc<<1);
    }
    return crc;
}
static uint32_t address(uint8_t slot)
{
    if(slot==0) return ((uint32_t)__builtin_tblpage(page_a)<<16)|__builtin_tbloffset(page_a);
    return ((uint32_t)__builtin_tblpage(page_b)<<16)|__builtin_tbloffset(page_b);
}
static bool read_slot(uint8_t slot)
{
    uint32_t a=address(slot); uint16_t i,save=TBLPAG;
    for(i=0;i<RECORD_WORDS;++i,a+=2) {
        TBLPAG=(uint16_t)(a>>16); record[i]=__builtin_tblrdl((uint16_t)a);
    }
    TBLPAG=save;
    return record[0]==0x4158 && record[1]==AUX_SCHEMA && record[5]==sizeof(aux_config_t)*AUX_PINS &&
        (record[4]==100 || record[4]==400 || record[4]==1000) && record[127]==checksum();
}
bool aux_nvm_load(aux_config_t *config,uint16_t *khz)
{
    uint8_t s; active=255; generation=0;
    for(s=0;s<2;++s) if(read_slot(s)) {
        uint32_t gen=record[2]|((uint32_t)record[3]<<16);
        if(active==255 || (int32_t)(gen-generation)>0) { generation=gen; active=s; }
    }
    if(active==255 || !read_slot(active)) return false;
    memcpy(config,&record[6],sizeof(aux_config_t)*AUX_PINS); *khz=record[4]; return true;
}
static bool commit_cycle(void)
{
    uint16_t sr=SR; SRbits.IPL=7;
    __builtin_write_NVM();
    __asm__ volatile("nop\n nop");
    while(NVMCONbits.WR) { }
    NVMCONbits.WREN=0; SR=sr;
    return NVMCONbits.WRERR==0;
}
bool aux_nvm_save(const aux_config_t *config,uint16_t khz)
{
    uint8_t slot=active==0 ? 1 : 0; uint32_t a=address(slot),base=a,gen=generation+1;
    uint16_t i,save=TBLPAG; bool ok=false;
    /* Compile-time layout guard keeps the record inside the first two rows. */
    typedef char config_fits[(sizeof(aux_config_t)*AUX_PINS <= (RECORD_WORDS-7U)*2U) ? 1 : -1];
    (void)sizeof(config_fits);
    memset(record,0xFF,sizeof record);
    record[0]=0x4158; record[1]=AUX_SCHEMA; record[2]=(uint16_t)gen; record[3]=(uint16_t)(gen>>16);
    record[4]=khz; record[5]=sizeof(aux_config_t)*AUX_PINS;
    memcpy(&record[6],config,sizeof(aux_config_t)*AUX_PINS); record[127]=checksum();
#if defined(__dsPIC33FJ32MC204__)
    NVMCON=0x4042; TBLPAG=(uint16_t)(a>>16); __builtin_tblwtl((uint16_t)a,0xFFFF);
#else
    NVMCON=0x4003; NVMADRU=(uint16_t)(a>>16); NVMADR=(uint16_t)a;
#endif
    if(!commit_cycle()) goto done;
#if defined(__dsPIC33FJ32MC204__)
    for(i=0;i<RECORD_WORDS;++i,a+=2) {
        TBLPAG=(uint16_t)(a>>16);
        __builtin_tblwtl((uint16_t)a,record[i]); __builtin_tblwth((uint16_t)a,0xFF);
        if((i&63U)==63U) { NVMCON=0x4001; if(!commit_cycle()) goto done; }
    }
#else
    for(i=0;i<RECORD_WORDS;i+=2,a+=4) {
        TBLPAG=0xFA;
        __builtin_tblwtl(0,record[i]); __builtin_tblwth(0,0xFF);
        __builtin_tblwtl(2,record[i+1]); __builtin_tblwth(2,0xFF);
        NVMADRU=(uint16_t)(a>>16); NVMADR=(uint16_t)a; NVMCON=0x4001;
        if(!commit_cycle()) goto done;
    }
#endif
    (void)base;
    ok=read_slot(slot);
    if(ok && (record[2]!=((uint16_t)gen) || record[3]!=(uint16_t)(gen>>16))) ok=false;
    if(ok) { active=slot; generation=gen; }
done:
    TBLPAG=save; return ok;
}
