#include <stdint.h>
#include "usb.h"
#include "usb_internal.h"
#include "io/port.h"
#include "io/pci.h"
#include "cpu.h"
#include "serial.h"
#include "io/io.h"
#include "usb_msc.h"
#include "mm/vmm.h"
#define MAX_CONTROLLERS 8
static USB_CONTROLLER controllers[MAX_CONTROLLERS];
static int controller_count;
static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t fn, uint8_t reg) {
    return PciConfigRead32(bus, slot, fn, reg);
}
uint32_t UsbPciRead(const USB_CONTROLLER *c, uint8_t r) { return pci_read(c->bus,c->slot,c->function,r); }
void UsbPciWrite(const USB_CONTROLLER *c, uint8_t r, uint32_t v) {
    PciConfigWrite32(c->bus, c->slot, c->function, r, v);
}
uint32_t UsbMmioRead32(uintptr_t b,uint32_t o){return *(volatile uint32_t*)(b+o);}
void UsbMmioWrite32(uintptr_t b,uint32_t o,uint32_t v){*(volatile uint32_t*)(b+o)=v;}
void UsbWait(uint32_t n){while(n--)CpuRelax();}
static const char *type_name(USB_HC_TYPE t){static const char*n[]={"UHCI","OHCI","EHCI","xHCI"};return n[t];}
void UsbLogPort(const USB_CONTROLLER*c,uint8_t p,int on){SerialPutString("[USB] ");SerialPutString(type_name(c->type));SerialPutString(" port ");SerialPrintDec(p+1);SerialPutString(on?" connected\r\n":" disconnected\r\n");}
static uintptr_t mmio_bar(USB_CONTROLLER*c){uint32_t l=UsbPciRead(c,0x10);uint64_t a=l&0xFFFFFFF0U;
#if defined(__loongarch64)
 if(!a){a=0x42000000U;UsbPciWrite(c,0x10,(uint32_t)a);l=UsbPciRead(c,0x10);a=l&0xFFFFFFF0U;}
#endif
#if UINTPTR_MAX > 0xFFFFFFFFU
 if((l&6)==4)a|=(uint64_t)UsbPciRead(c,0x14)<<32;
#endif
 return (uintptr_t)VmmMapMmioRange(a,0x10000);}
static int start(USB_CONTROLLER*c){if(c->type==USB_HC_UHCI)return UhciInitialize(c);if(c->type==USB_HC_OHCI)return OhciInitialize(c);if(c->type==USB_HC_EHCI)return EhciInitialize(c);return XhciInitialize(c);}
void UsbInit(void){controller_count=0;
#if defined(__loongarch64)
 const uint16_t max_bus=1;
#else
 const uint16_t max_bus=256;
#endif
 for(uint16_t b=0;b<max_bus&&controller_count<MAX_CONTROLLERS;b++)for(uint8_t s=0;s<32&&controller_count<MAX_CONTROLLERS;s++)for(uint8_t f=0;f<8&&controller_count<MAX_CONTROLLERS;f++){
  uint32_t id=pci_read((uint8_t)b,s,f,0);if(id==0xFFFFFFFFU){if(!f)break;continue;}uint32_t cc=pci_read((uint8_t)b,s,f,8);if((cc>>24)!=0x0C||((cc>>16)&255)!=3)continue;
  USB_CONTROLLER*c=&controllers[controller_count];c->bus=b;c->slot=s;c->function=f;c->base=0;c->io_base=0;c->ports=0;c->private_data=0;uint8_t pi=cc>>8;
  if(pi==0){c->type=USB_HC_UHCI;c->io_base=UsbPciRead(c,0x20)&0xFFFC;}else if(pi==0x10)c->type=USB_HC_OHCI;else if(pi==0x20)c->type=USB_HC_EHCI;else if(pi==0x30)c->type=USB_HC_XHCI;else continue;
  if(c->type!=USB_HC_UHCI)c->base=mmio_bar(c);UsbPciWrite(c,4,UsbPciRead(c,4)|6);SerialPutString("[USB] Found ");SerialPutString(type_name(c->type));SerialPutString(" controller at 0x");
#if UINTPTR_MAX > 0xFFFFFFFFU
  SerialPrintHex((uint32_t)((uint64_t)c->base>>32));
#endif
  SerialPrintHex((uint32_t)(c->base?c->base:c->io_base));SerialPutString("\r\n");if(start(c)){c->initialized=1;controller_count++;}
 }SerialPutString("[USB] ");SerialPrintDec(controller_count);SerialPutString(" host controller(s) ready\r\n");}
void UsbPoll(void){for(int i=0;i<controller_count;i++){USB_CONTROLLER*c=&controllers[i];if(c->type==USB_HC_UHCI)UhciPoll(c);else if(c->type==USB_HC_OHCI)OhciPoll(c);else if(c->type==USB_HC_EHCI)EhciPoll(c);else XhciPoll(c);}}
int UsbIsReady(void){return controller_count>0;}int UsbGetControllerCount(void){return controller_count;}
int UsbBootInitialize(void){IO_DRIVER_OBJECT*d=IoCreateDriver("BootUsb",0,0);if(!d)return 0;UsbMscInitialize(d);UsbInit();return controller_count>0;}
int DriverEntry(IO_DRIVER_OBJECT*d,void*x){(void)x;UsbMscInitialize(d);UsbInit();return controller_count>0;}
