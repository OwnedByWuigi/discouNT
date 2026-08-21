#ifndef DISCOUNT_IO_PORT_H
#define DISCOUNT_IO_PORT_H
#include <stdint.h>

#if defined(__loongarch64)
#define IO_PORT_CPU_BASE 0x18000000UL
static inline uint8_t inb(uint16_t p){return p<0x4000?0xff:*(volatile uint8_t*)(IO_PORT_CPU_BASE+p);}
static inline uint16_t inw(uint16_t p){return p<0x4000?0xffff:*(volatile uint16_t*)(IO_PORT_CPU_BASE+p);}
static inline uint32_t inl(uint16_t p){return p<0x4000?0xffffffffU:*(volatile uint32_t*)(IO_PORT_CPU_BASE+p);}
static inline void outb(uint16_t p,uint8_t v){if(p>=0x4000)*(volatile uint8_t*)(IO_PORT_CPU_BASE+p)=v;}
static inline void outw(uint16_t p,uint16_t v){if(p>=0x4000)*(volatile uint16_t*)(IO_PORT_CPU_BASE+p)=v;}
static inline void outl(uint16_t p,uint32_t v){if(p>=0x4000)*(volatile uint32_t*)(IO_PORT_CPU_BASE+p)=v;}
#else
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0, %1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1, %0":"=a"(v):"Nd"(p));return v;}
static inline void outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0, %1"::"a"(v),"Nd"(p));}
static inline uint16_t inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1, %0":"=a"(v):"Nd"(p));return v;}
static inline void outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0, %1"::"a"(v),"Nd"(p));}
static inline uint32_t inl(uint16_t p){uint32_t v;__asm__ volatile("inl %1, %0":"=a"(v):"Nd"(p));return v;}
#endif
#endif
