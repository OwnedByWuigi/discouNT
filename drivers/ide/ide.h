#ifndef IDE_H
#define IDE_H

#include "io.h"

/* Probe the two legacy IDE channels and publish ATA disks as Harddisk0..3. */
int IdeBootInitialize(void);
int IdeInitialize(IO_DRIVER_OBJECT *driver);
uint32_t IdeGetDiskCount(void);
uint32_t IdeGetDiskSectors(uint32_t index);

#endif
