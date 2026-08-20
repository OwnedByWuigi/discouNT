#include "usb_internal.h"
int EhciInitialize(USB_CONTROLLER*c){if(!c->base)return 0;uint8_t l=*(volatile uint8_t*)c->base;uintptr_t o=c->base+l;c->ports=UsbMmioRead32(c->base,4)&15;UsbMmioWrite32(o,0,UsbMmioRead32(o,0)|2);UsbWait(200000);UsbMmioWrite32(o,4,0xFFFFFFFF);for(uint8_t p=0;p<c->ports;p++){uint32_t s=UsbMmioRead32(o,0x44+p*4);if(s&1)UsbLogPort(c,p,1);UsbMmioWrite32(o,0x44+p*4,s|6);}UsbMmioWrite32(o,0x40,1);return 1;}
void EhciPoll(USB_CONTROLLER*c){(void)c;}
