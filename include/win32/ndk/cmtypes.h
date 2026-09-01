#ifndef DISCOUNT_CMTYPES_H
#define DISCOUNT_CMTYPES_H
#include <windows.h>

typedef struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
    BYTE Type, ShareDisposition;
    WORD Flags;
    union {
        struct { LARGE_INTEGER Start; ULONG Length; } Port;
        struct { ULONG Level, Vector, Affinity; } Interrupt;
        struct { LARGE_INTEGER Start; ULONG Length; } Memory;
        struct { UCHAR Channel, Port, Reserved1, Reserved2; } Dma;
        struct { ULONG Reserved1, Reserved2, DataSize; } DeviceSpecificData;
    } u;
} CM_PARTIAL_RESOURCE_DESCRIPTOR, *PCM_PARTIAL_RESOURCE_DESCRIPTOR;
typedef struct _CM_PARTIAL_RESOURCE_LIST {
    WORD Version, Revision;
    ULONG Count;
    CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[1];
} CM_PARTIAL_RESOURCE_LIST, *PCM_PARTIAL_RESOURCE_LIST;
typedef struct _CM_FULL_RESOURCE_DESCRIPTOR {
    ULONG InterfaceType, BusNumber;
    CM_PARTIAL_RESOURCE_LIST PartialResourceList;
} CM_FULL_RESOURCE_DESCRIPTOR, *PCM_FULL_RESOURCE_DESCRIPTOR;
typedef struct _CM_RESOURCE_LIST {
    ULONG Count;
    CM_FULL_RESOURCE_DESCRIPTOR List[1];
} CM_RESOURCE_LIST, *PCM_RESOURCE_LIST;
typedef struct _IO_RESOURCE_DESCRIPTOR {
    BYTE Option, Type, ShareDisposition, Spare1;
    WORD Flags, Spare2;
    union {
        struct { LARGE_INTEGER Length, Alignment, MinimumAddress, MaximumAddress; } Port;
        struct { LARGE_INTEGER Length, Alignment, MinimumAddress, MaximumAddress; } Memory;
        struct { ULONG MinimumVector, MaximumVector; } Interrupt;
        struct { ULONG MinimumChannel, MaximumChannel; } Dma;
    } u;
} IO_RESOURCE_DESCRIPTOR, *PIO_RESOURCE_DESCRIPTOR;
typedef struct _IO_RESOURCE_LIST {
    WORD Version, Revision;
    ULONG Count;
    IO_RESOURCE_DESCRIPTOR Descriptors[1];
} IO_RESOURCE_LIST, *PIO_RESOURCE_LIST;
typedef struct _IO_RESOURCE_REQUIREMENTS_LIST {
    ULONG ListSize, InterfaceType, BusNumber, SlotNumber;
    ULONG Reserved[3];
    ULONG AlternativeLists;
    IO_RESOURCE_LIST List[1];
} IO_RESOURCE_REQUIREMENTS_LIST, *PIO_RESOURCE_REQUIREMENTS_LIST;
#endif
