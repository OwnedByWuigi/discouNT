#include "usb_internal.h"
#include "arch/x86/portio.h"
int UhciInitialize(USB_CONTROLLER*c){if(!c->io_base)return 0;outw(c->io_base,4);UsbWait(100000);outw(c->io_base,0);outw(c->io_base+2,0xFFFF);c->ports=2;for(uint8_t p=0;p<2;p++){uint16_t s=inw(c->io_base+0x10+p*2);if(s&1)UsbLogPort(c,p,1);outw(c->io_base+0x10+p*2,s|0xA);}return 1;}
void UhciPoll(USB_CONTROLLER*c){(void)c;}
