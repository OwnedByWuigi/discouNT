#ifndef USB_H
#define USB_H

#include <stdint.h>

void UsbInit(void);
void UsbPoll(void);
int UsbIsReady(void);
int UsbGetControllerCount(void);
int UsbBootInitialize(void);

#endif
