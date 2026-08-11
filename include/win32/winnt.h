#ifndef DISCOUNT_WINNT_H
#define DISCOUNT_WINNT_H

#include "windef.h"

typedef LONG NTSTATUS;
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
typedef uint32_t ACCESS_MASK;

typedef struct _LARGE_INTEGER {
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _ULARGE_INTEGER {
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _LUID {
    DWORD LowPart;
    LONG  HighPart;
} LUID, *PLUID;

typedef struct _LUID_AND_ATTRIBUTES {
    LUID  Luid;
    DWORD Attributes;
} LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;

typedef struct _TOKEN_PRIVILEGES {
    DWORD PrivilegeCount;
    LUID_AND_ATTRIBUTES Privileges[1];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct _CRITICAL_SECTION {
    void *DebugInfo;
    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

typedef struct _IO_COUNTERS {
    ULONGLONG ReadOperationCount;
    ULONGLONG WriteOperationCount;
    ULONGLONG OtherOperationCount;
    ULONGLONG ReadTransferCount;
    ULONGLONG WriteTransferCount;
    ULONGLONG OtherTransferCount;
} IO_COUNTERS, *PIO_COUNTERS;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef LONG KPRIORITY;

#define STATUS_SUCCESS            ((NTSTATUS)0x00000000)
#define STATUS_NOT_IMPLEMENTED    ((NTSTATUS)0xC0000002)
#define STATUS_NO_MORE_ENTRIES    ((NTSTATUS)0x8000001A)

#define PROCESS_TERMINATE            0x0001
#define PROCESS_CREATE_THREAD        0x0002
#define PROCESS_SET_SESSIONID        0x0004
#define PROCESS_VM_OPERATION         0x0008
#define PROCESS_VM_READ              0x0010
#define PROCESS_VM_WRITE             0x0020
#define PROCESS_DUP_HANDLE           0x0040
#define PROCESS_CREATE_PROCESS       0x0080
#define PROCESS_SET_QUOTA            0x0100
#define PROCESS_SET_INFORMATION      0x0200
#define PROCESS_QUERY_INFORMATION    0x0400
#define PROCESS_SUSPEND_RESUME       0x0800
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define PROCESS_ALL_ACCESS           0x1FFFFF

#define TOKEN_QUERY                  0x0008
#define TOKEN_DUPLICATE              0x0002
#define TOKEN_IMPERSONATE            0x0004
#define TOKEN_ADJUST_PRIVILEGES      0x0020

#define SE_PRIVILEGE_ENABLED         0x00000002

#define HIGH_PRIORITY_CLASS          0x00000080
#define IDLE_PRIORITY_CLASS          0x00000040
#define NORMAL_PRIORITY_CLASS        0x00000020
#define REALTIME_PRIORITY_CLASS      0x00000100
#define BELOW_NORMAL_PRIORITY_CLASS  0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS  0x00008000

#define WAIT_OBJECT_0                0x00000000
#define WAIT_TIMEOUT                 0x00000102

#endif
