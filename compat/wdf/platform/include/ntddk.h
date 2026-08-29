#ifndef DISCOUNT_WDF_NTDDK_H
#define DISCOUNT_WDF_NTDDK_H

#include <stdint.h>
#include <stddef.h>
#include <driverspecs.h>
#include "../../../../include/win32/winnt.h"
#include "../../../../include/win32/guiddef.h"
#include <ntstatus.h>

/* The Win32 header exposes this as a macro; WDM exposes it as a callable
 * platform primitive so framework objects can take its address. */
#undef RtlCopyMemory

#ifndef NT_PROCESSOR_GROUPS
#define NT_PROCESSOR_GROUPS 1
#endif
#ifndef ANYSIZE_ARRAY
#define ANYSIZE_ARRAY 1
#endif
#ifndef FORCEINLINE
#define FORCEINLINE static inline
#endif
#ifndef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT
#endif
#ifndef DECLSPEC_EXPORT
#define DECLSPEC_EXPORT
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef PAGED_CODE
#define PAGED_CODE() ((void)0)
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#endif
#ifndef CONST
#define CONST const
#endif

typedef char CCHAR;
typedef int16_t CSHORT;
typedef uint8_t KIRQL;
typedef uint8_t KPROCESSOR_MODE;
typedef uintptr_t KAFFINITY;
typedef uintptr_t PFN_NUMBER;
typedef LARGE_INTEGER PHYSICAL_ADDRESS;
typedef void *PETHREAD;
typedef void *PEPROCESS;
typedef void *PSECTION_OBJECT;
typedef void *PSECURITY_DESCRIPTOR;
typedef void *PACCESS_TOKEN;
typedef void *PVOID64;
typedef char *PCHAR;
typedef const char *PCCHAR;
typedef uint8_t *PUCHAR;
typedef uint16_t *PUSHORT;
typedef uint32_t *PULONG;
typedef uint64_t *PULONG64;
typedef int32_t *PLONG;
typedef uint64_t ULONG64;
typedef uint64_t *PULONGLONG;
typedef uint32_t DEVICE_TYPE;
typedef uint32_t POWER_ACTION;
typedef uint32_t POWER_STATE_TYPE;
typedef uint32_t POOL_TYPE;
typedef uintptr_t KSPIN_LOCK;
typedef uint32_t LOCK_QUEUE_HANDLE;
typedef uint32_t KWAIT_REASON;
typedef uint32_t KWAIT_BLOCK;
typedef uint32_t KBUGCHECK_CALLBACK_REASON;
typedef uint32_t KINTERRUPT_MODE;
typedef uint32_t KINTERRUPT_POLARITY;

typedef struct _KDPC KDPC, *PKDPC;
typedef struct _KTIMER KTIMER, *PKTIMER;
typedef struct _KEVENT KEVENT, *PKEVENT;
typedef struct _MDL MDL, *PMDL;
typedef struct _IRP IRP, *PIRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION, *PIO_STACK_LOCATION;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

typedef NTSTATUS DRIVER_DISPATCH(PDEVICE_OBJECT DeviceObject, PIRP Irp);
typedef DRIVER_DISPATCH *PDRIVER_DISPATCH;
typedef NTSTATUS DRIVER_INITIALIZE(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
typedef DRIVER_INITIALIZE *PDRIVER_INITIALIZE;
typedef VOID DRIVER_STARTIO(PDEVICE_OBJECT DeviceObject, PIRP Irp);
typedef DRIVER_STARTIO *PDRIVER_STARTIO;
typedef VOID DRIVER_UNLOAD(PDRIVER_OBJECT DriverObject);
typedef DRIVER_UNLOAD *PDRIVER_UNLOAD;
typedef NTSTATUS IO_COMPLETION_ROUTINE(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
typedef IO_COMPLETION_ROUTINE *PIO_COMPLETION_ROUTINE;

typedef struct _IO_STATUS_BLOCK {
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _KEVENT {
    int32_t Type;
    int32_t SignalState;
} KEVENT;

typedef struct _KDPC {
    PVOID DeferredContext;
    PVOID DeferredRoutine;
    uint8_t Inserted;
} KDPC;

typedef struct _KTIMER {
    int32_t Type;
    int32_t SignalState;
    LIST_ENTRY TimerListEntry;
} KTIMER;

typedef struct _MDL {
    struct _MDL *Next;
    SHORT Size;
    SHORT MdlFlags;
    PVOID StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
} MDL;

typedef struct _DEVICE_OBJECT {
    SHORT Type;
    USHORT Size;
    LONG ReferenceCount;
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT NextDevice;
    PDEVICE_OBJECT AttachedDevice;
    PVOID DeviceExtension;
    ULONG Flags;
    ULONG Characteristics;
    ULONG DeviceType;
    UCHAR StackSize;
    union { LIST_ENTRY ListEntry; } Queue;
    ULONG AlignmentRequirement;
    KSPIN_LOCK DeviceLock;
    ULONG ActiveThreadCount;
} DEVICE_OBJECT;

typedef struct _DRIVER_OBJECT {
    SHORT Type;
    SHORT Size;
    PDEVICE_OBJECT DeviceObject;
    ULONG Flags;
    PVOID DriverStart;
    ULONG DriverSize;
    PVOID DriverSection;
    PDRIVER_OBJECT AttachmentDriver;
    PUNICODE_STRING DriverName;
    PUNICODE_STRING HardwareDatabase;
    PVOID FastIoDispatch;
    PDRIVER_INITIALIZE DriverInit;
    PDRIVER_STARTIO DriverStartIo;
    PDRIVER_UNLOAD DriverUnload;
    PDRIVER_DISPATCH MajorFunction[28];
} DRIVER_OBJECT;

typedef struct _IRP {
    CSHORT Type;
    USHORT Size;
    PMDL MdlAddress;
    ULONG Flags;
    union { PVOID UserBuffer; } AssociatedIrp;
    IO_STATUS_BLOCK IoStatus;
    KPROCESSOR_MODE RequestorMode;
    BOOLEAN PendingReturned;
    BOOLEAN Cancel;
    PVOID CancelRoutine;
    PVOID UserEvent;
    PVOID UserIosb;
    PVOID Tail;
    IO_STACK_LOCATION *CurrentStackLocation;
} IRP;

typedef enum _DEVICE_RELATION_TYPE {
    BusRelations = 0,
    EjectionRelations,
    PowerRelations,
    RemovalRelations,
    TargetDeviceRelation,
    SingleBusRelations,
    TransportRelations
} DEVICE_RELATION_TYPE;

typedef enum _DEVICE_POWER_STATE {
    PowerDeviceUnspecified = 0,
    PowerDeviceD0, PowerDeviceD1, PowerDeviceD2, PowerDeviceD3,
    PowerDeviceMaximum
} DEVICE_POWER_STATE;

typedef enum _SYSTEM_POWER_STATE {
    PowerSystemUnspecified = 0,
    PowerSystemWorking, PowerSystemSleeping1, PowerSystemSleeping2,
    PowerSystemSleeping3, PowerSystemHibernate, PowerSystemShutdown,
    PowerSystemMaximum
} SYSTEM_POWER_STATE;

typedef struct _DEVICE_CAPABILITIES {
    USHORT Size;
    USHORT Version;
    ULONG DeviceD1 : 1;
    ULONG DeviceD2 : 1;
    ULONG LockSupported : 1;
    ULONG EjectSupported : 1;
    ULONG Removable : 1;
    ULONG DockDevice : 1;
    ULONG UniqueID : 1;
    ULONG SilentInstall : 1;
    ULONG RawDeviceOK : 1;
    ULONG SurpriseRemovalOK : 1;
    ULONG WakeFromD0 : 1;
    ULONG WakeFromD1 : 1;
    ULONG WakeFromD2 : 1;
    ULONG WakeFromD3 : 1;
    ULONG HardwareDisabled : 1;
    ULONG NonDynamic : 1;
    ULONG Address;
    ULONG UINumber;
    DEVICE_POWER_STATE DeviceState[PowerSystemMaximum];
    SYSTEM_POWER_STATE SystemWake;
    DEVICE_POWER_STATE DeviceWake;
    ULONG D1Latency;
    ULONG D2Latency;
    ULONG D3Latency;
} DEVICE_CAPABILITIES, *PDEVICE_CAPABILITIES;

typedef struct _IO_STACK_LOCATION {
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    UCHAR Flags;
    UCHAR Control;
    union {
        struct { PIO_COMPLETION_ROUTINE CompletionRoutine; PVOID Context; } Completion;
        struct { ULONG IoControlCode; PVOID Type3InputBuffer; ULONG InputBufferLength; ULONG OutputBufferLength; } DeviceIoControl;
        struct { ULONG Options; USHORT FileAttributes; USHORT ShareAccess; PVOID EaLength; } Create;
        struct { ULONG Length; ULONG Key; LARGE_INTEGER ByteOffset; } Read;
        struct { ULONG Length; ULONG Key; LARGE_INTEGER ByteOffset; } Write;
        struct { DEVICE_RELATION_TYPE Type; } QueryDeviceRelations;
    } Parameters;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
} IO_STACK_LOCATION;

#define DO_DEVICE_INITIALIZING 0x00000080UL
#define DO_BUFFERED_IO         0x00000004UL
#define DO_DIRECT_IO           0x00000002UL
#define FILE_DEVICE_UNKNOWN    0x00000022UL
#define FILE_DEVICE_DISK       0x00000007UL
#define FILE_DEVICE_SOUND      0x0000001dUL
#define FILE_AUTOGENERATED_DEVICE_NAME 0x00000080UL
#define IRP_MJ_CREATE 0
#define IRP_MJ_CLOSE 2
#define IRP_MJ_READ 3
#define IRP_MJ_WRITE 4
#define IRP_MJ_DEVICE_CONTROL 14
#define IRP_MJ_INTERNAL_DEVICE_CONTROL 15
#define IRP_MJ_PNP 27
#define IRP_MJ_POWER 22
#define IRP_MN_START_DEVICE 0
#define IRP_MN_REMOVE_DEVICE 2
#define NonPagedPool 0
#define NonPagedPoolNx 0
#define PagedPool 1
#define KernelMode 0
#define UserMode 1
#define Executive 0
#define NotificationEvent 0
#define SynchronizationEvent 1
#define IO_NO_INCREMENT 0

#define IoGetCurrentIrpStackLocation(Irp) ((Irp)->CurrentStackLocation)
#define IoSkipCurrentIrpStackLocation(Irp) ((void)0)
#define IoMarkIrpPending(Irp) ((Irp)->PendingReturned = 1)
#define IoCompleteRequest(Irp,PriorityBoost) ((void)(Irp),(void)(PriorityBoost))

PVOID ExAllocatePoolWithTag(POOL_TYPE PoolType, SIZE_T NumberOfBytes, ULONG Tag);
PVOID ExAllocatePool2(ULONG Flags, SIZE_T NumberOfBytes, ULONG Tag);
VOID ExFreePool(PVOID P);
VOID ExFreePoolWithTag(PVOID P, ULONG Tag);
VOID RtlZeroMemory(PVOID Destination, SIZE_T Length);
VOID RtlCopyMemory(PVOID Destination, CONST PVOID Source, SIZE_T Length);
LONG InterlockedIncrement(volatile LONG *Addend);
LONG InterlockedDecrement(volatile LONG *Addend);
LONG InterlockedExchange(volatile LONG *Target, LONG Value);
LONG InterlockedCompareExchange(volatile LONG *Destination, LONG Exchange, LONG Comperand);
VOID KeInitializeEvent(PKEVENT Event, int Type, BOOLEAN State);
LONG KeSetEvent(PKEVENT Event, LONG Increment, BOOLEAN Wait);
LONG KeResetEvent(PKEVENT Event);
LONG KeReadStateEvent(PKEVENT Event);
VOID KeInitializeDpc(PKDPC Dpc, PVOID Routine, PVOID Context);
VOID KeInitializeSpinLock(KSPIN_LOCK *SpinLock);
VOID KeAcquireSpinLock(KSPIN_LOCK *SpinLock, KIRQL *OldIrql);
VOID KeReleaseSpinLock(KSPIN_LOCK *SpinLock, KIRQL OldIrql);
VOID KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK *SpinLock);
VOID KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK *SpinLock);
VOID KeStallExecutionProcessor(ULONG Microseconds);

#endif
