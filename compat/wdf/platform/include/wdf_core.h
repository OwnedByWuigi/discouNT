#ifndef DISCOUNT_WDF_CORE_H
#define DISCOUNT_WDF_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <ntddk.h>

/*
 * The public KMDF headers describe handles as opaque pointers.  Keeping the
 * implementation opaque here is important: drivers must not depend on the
 * layout of a framework object.
 */
typedef void *WDFCORE_HANDLE;
typedef void (*WDFCORE_CALLBACK)(void);
typedef WDFCORE_CALLBACK WDFFUNC;

typedef struct _WDF_DRIVER_GLOBALS {
    WDFCORE_HANDLE Driver;
    ULONG DriverFlags;
    ULONG DriverTag;
    CHAR DriverName[32];
    BOOLEAN DisplaceDriverUnload;
} WDF_DRIVER_GLOBALS, *PWDF_DRIVER_GLOBALS;

extern const WDFFUNC *WdfFunctions_01033;
extern ULONG WdfFunctionCount;
extern WDFFUNC WdfDriverMiniportUnloadOverride;

void WdfPlatformInitialize(void);

#endif
