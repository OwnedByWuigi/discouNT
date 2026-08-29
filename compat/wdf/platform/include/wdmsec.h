#ifndef DISCOUNT_WDF_WDMSEC_H
#define DISCOUNT_WDF_WDMSEC_H
#include <ntddk.h>
typedef ULONG ACCESS_MASK;
#define IoReadAccess  0x0001
#define IoWriteAccess 0x0002
#define IoModifyAccess 0x0004
#endif
