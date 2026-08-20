#ifndef USB_INTERNAL_H
#define USB_INTERNAL_H
#include <stdint.h>
typedef enum { USB_HC_UHCI, USB_HC_OHCI, USB_HC_EHCI, USB_HC_XHCI } USB_HC_TYPE;
typedef struct _USB_CONTROLLER {
    uint8_t bus, slot, function;
    USB_HC_TYPE type;
    uintptr_t base;
    uint16_t io_base;
    uint8_t ports, initialized;
    void *private_data;
} USB_CONTROLLER;
uint32_t UsbPciRead(const USB_CONTROLLER *, uint8_t);
void UsbPciWrite(const USB_CONTROLLER *, uint8_t, uint32_t);
uint32_t UsbMmioRead32(uintptr_t, uint32_t);
void UsbMmioWrite32(uintptr_t, uint32_t, uint32_t);
void UsbWait(uint32_t);
void UsbLogPort(const USB_CONTROLLER *, uint8_t, int);
int UhciInitialize(USB_CONTROLLER *); void UhciPoll(USB_CONTROLLER *);
int OhciInitialize(USB_CONTROLLER *); void OhciPoll(USB_CONTROLLER *);
int EhciInitialize(USB_CONTROLLER *); void EhciPoll(USB_CONTROLLER *);
int XhciInitialize(USB_CONTROLLER *); void XhciPoll(USB_CONTROLLER *);
#endif
