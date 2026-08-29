#ifndef DISCOUNT_WDF_PLATFORM_H
#define DISCOUNT_WDF_PLATFORM_H

/*
 * The WDF source uses this header as the seam between framework code and the
 * operating-system implementation.  Keep it independent from discouNT's
 * Win32 surface: WDF code sees WDM contracts, while the implementation can
 * bind those contracts to kernel/ob, kernel/mm, and kernel/io later.
 */
#include <ntddk.h>

#define FX_CORE_KERNEL_MODE 1
#define FX_CORE_USER_MODE   2

typedef struct _WDF_PLATFORM_DRIVER_BINDING {
    PDRIVER_OBJECT DriverObject;
    PUNICODE_STRING RegistryPath;
    PVOID PlatformContext;
} WDF_PLATFORM_DRIVER_BINDING, *PWDF_PLATFORM_DRIVER_BINDING;

#endif
