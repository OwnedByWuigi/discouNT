#ifndef DRIVER_H
#define DRIVER_H

#include <stdint.h>

void DriverLoadAll(void *mb_info);
void *DriverResolveSymbol(const char *name);

void DriverInstallSerial(void *image);
void DriverInstallVga(void *image);
void DriverInstallCdfs(void *image);
void DriverInstallKeyboard(void *image);
void DriverInstallMouse(void *image);
void DriverInstallNet(void *image);
void DriverInstallFb(void *image);
void DriverInstallWin32k(void *image);

#endif
