#ifndef DISCOUNT_WDF_UMDF_H
#define DISCOUNT_WDF_UMDF_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t WDF_UMDF_NTSTATUS;
typedef void *WDF_UMDF_HANDLE;
typedef void (*WDF_UMDF_FUNC)(void);

typedef struct _WDF_UMDF_GLOBALS {
    WDF_UMDF_HANDLE Driver;
    uint32_t DriverFlags;
    uint32_t DriverTag;
    char DriverName[32];
    uint8_t DisplaceDriverUnload;
} WDF_UMDF_GLOBALS;

extern WDF_UMDF_GLOBALS *WdfDriverGlobals;
extern const WDF_UMDF_FUNC *WdfFunctions_02017;

/* Host entry point for a user-mode driver host.  The request and buffer are
 * host-owned and remain valid until the callback returns. */
WDF_UMDF_NTSTATUS WudfHostDispatch(WDF_UMDF_HANDLE device, uint32_t major,
                                   void *buffer, uint32_t length,
                                   uint32_t ioctl_code, const void *input,
                                   uint32_t input_length, uint32_t *information);

#endif
