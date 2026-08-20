#include "usb_internal.h"
int OhciInitialize(USB_CONTROLLER*c){if(!c->base)return 0;uint32_t r=UsbMmioRead32(c->base,0);if(!(r&255)||r==0xFFFFFFFF)return 0;UsbMmioWrite32(c->base,4,0);UsbMmioWrite32(c->base,8,1);UsbWait(100000);UsbMmioWrite32(c->base,0xC,0xFFFFFFFF);c->ports=UsbMmioRead32(c->base,0x48)&255;if(c->ports>15)c->ports=15;for(uint8_t p=0;p<c->ports;p++){uint32_t s=UsbMmioRead32(c->base,0x54+p*4);if(s&1)UsbLogPort(c,p,1);UsbMmioWrite32(c->base,0x54+p*4,s|0x30000);}UsbMmioWrite32(c->base,4,0x80);return 1;}
void OhciPoll(USB_CONTROLLER*c){(void)c;}
