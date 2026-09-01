#ifndef DISCOUNT_WINNT_H
#define DISCOUNT_WINNT_H

#include "windef.h"
#define CONTAINING_RECORD(address,type,field) ((type *)((BYTE *)(address)-__builtin_offsetof(type,field)))
#define FIELD_OFFSET(type,field) ((LONG)__builtin_offsetof(type,field))
#define LongToHandle(value) ((HANDLE)(LONG_PTR)(LONG)(value))
#define UlongToHandle(value) ((HANDLE)(ULONG_PTR)(ULONG)(value))
#define MAXLONG 0x7fffffff

typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;
typedef char CHAR;
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
typedef uint32_t ACCESS_MASK;
typedef ACCESS_MASK REGSAM;

typedef struct _LARGE_INTEGER {
    union { struct { DWORD LowPart; LONG HighPart; } u; LONGLONG QuadPart; };
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _ULARGE_INTEGER {
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef struct _ANSI_STRING {
    USHORT Length;
    USHORT MaximumLength;
    char *Buffer;
} ANSI_STRING, *PANSI_STRING;

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

typedef void *PSID;
typedef struct _SID_IDENTIFIER_AUTHORITY { BYTE Value[6]; } SID_IDENTIFIER_AUTHORITY, *PSID_IDENTIFIER_AUTHORITY;
typedef struct _SID_AND_ATTRIBUTES { PSID Sid; DWORD Attributes; } SID_AND_ATTRIBUTES, *PSID_AND_ATTRIBUTES;
typedef struct _SID { BYTE Revision; BYTE SubAuthorityCount; SID_IDENTIFIER_AUTHORITY IdentifierAuthority; DWORD SubAuthority[1]; } SID, *PSID_STRUCT;
typedef struct _TOKEN_USER { SID_AND_ATTRIBUTES User; } TOKEN_USER, *PTOKEN_USER;
typedef struct _TOKEN_GROUPS { DWORD GroupCount; SID_AND_ATTRIBUTES Groups[1]; } TOKEN_GROUPS, *PTOKEN_GROUPS;
typedef enum _TOKEN_INFORMATION_CLASS { TokenUser = 1, TokenGroups = 2, TokenPrivileges = 3, TokenStatistics = 10 } TOKEN_INFORMATION_CLASS;
typedef enum _SECURITY_IMPERSONATION_LEVEL { SecurityAnonymous, SecurityIdentification, SecurityImpersonation, SecurityDelegation } SECURITY_IMPERSONATION_LEVEL;
typedef enum _TOKEN_TYPE { TokenPrimary = 1, TokenImpersonation = 2 } TOKEN_TYPE;
typedef struct _TOKEN_SOURCE { CHAR SourceName[8]; LUID SourceIdentifier; } TOKEN_SOURCE, *PTOKEN_SOURCE;
typedef struct _TOKEN_STATISTICS { LUID TokenId; LUID AuthenticationId; LARGE_INTEGER ExpirationTime; DWORD TokenType; DWORD ImpersonationLevel; DWORD DynamicCharged; DWORD DynamicAvailable; DWORD GroupCount; DWORD PrivilegeCount; LUID ModifiedId; } TOKEN_STATISTICS, *PTOKEN_STATISTICS;

typedef struct _TOKEN_PRIVILEGES {
    DWORD PrivilegeCount;
    LUID_AND_ATTRIBUTES Privileges[1];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct _RTL_CRITICAL_SECTION_DEBUG {
    WORD Type;
    WORD CreatorBackTraceIndex;
    struct _CRITICAL_SECTION *CriticalSection;
    LIST_ENTRY ProcessLocksList;
    DWORD EntryCount;
    DWORD ContentionCount;
    DWORD Flags;
    WORD CreatorBackTraceIndexHigh;
    WORD SpareWORD;
    ULONG_PTR Spare[2];
} RTL_CRITICAL_SECTION_DEBUG, *PRTL_CRITICAL_SECTION_DEBUG;

typedef struct _CRITICAL_SECTION {
    PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;
#define RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO 0x01000000

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
#define STATUS_BUFFER_TOO_SMALL   ((NTSTATUS)0xC0000023)
#define SECURITY_NT_AUTHORITY     {0,0,0,0,0,5}
#define SECURITY_BUILTIN_DOMAIN_RID 0x20
#define DOMAIN_ALIAS_RID_ADMINS   0x220
#define SECURITY_NULL_RID         0
#define SE_GROUP_MANDATORY        0x00000001
#define SE_GROUP_ENABLED          0x00000004
#define SE_GROUP_ENABLED_BY_DEFAULT 0x00000002
#define SE_GROUP_OWNER            0x00000008
#define SE_GROUP_LOGON_ID         0xC0000000
#define MAXIMUM_ALLOWED           0x02000000
#define RtlCopyMemory(d,s,n) memcpy((d),(s),(n))
#define STATUS_LOGON_FAILURE      ((NTSTATUS)0xC000006D)
#define STATUS_ACCOUNT_RESTRICTION ((NTSTATUS)0xC000006E)
#define STATUS_ACCOUNT_DISABLED    ((NTSTATUS)0xC0000072)
#define STATUS_ACCOUNT_LOCKED_OUT  ((NTSTATUS)0xC0000234)
#define STATUS_PASSWORD_MUST_CHANGE ((NTSTATUS)0xC0000224)
#define STATUS_PASSWORD_EXPIRED    ((NTSTATUS)0xC0000071)
#define STATUS_ACCOUNT_EXPIRED     ((NTSTATUS)0xC0000193)
#define STATUS_INVALID_LOGON_HOURS ((NTSTATUS)0xC000006F)
#define STATUS_INVALID_WORKSTATION ((NTSTATUS)0xC0000070)

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
