#ifndef USB_MSC_H
#define USB_MSC_H
#include <stdint.h>
#include "io.h"
typedef int (*USB_MSC_BULK_TRANSFER)(void *context, uint8_t endpoint,
                                    void *buffer, uint32_t length);
void UsbMscInitialize(IO_DRIVER_OBJECT *driver);
IO_DEVICE_OBJECT *UsbMscAttach(void *transport_context,
                               USB_MSC_BULK_TRANSFER transfer,
                               uint8_t bulk_in, uint8_t bulk_out);
#endif
