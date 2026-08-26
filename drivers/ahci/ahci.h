#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include "io/io.h"

int AhciBootInitialize(void);
int AhciInitialize(struct _IO_DRIVER_OBJECT *driver);
uint32_t AhciGetDiskCount(void);
uint32_t AhciGetDiskSectors(uint32_t index);
const char *AhciGetDiskName(uint32_t index);

#endif
