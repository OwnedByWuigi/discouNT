#ifndef DISCOUNT_MSGINA_RTLFUNCS_H
#define DISCOUNT_MSGINA_RTLFUNCS_H
#include "winnt.h"
void WINAPI RtlInitUnicodeString(PUNICODE_STRING dst, PCWSTR src);
BOOL WINAPI RtlEqualUnicodeString(const UNICODE_STRING *a, const UNICODE_STRING *b, BOOL insensitive);
NTSTATUS WINAPI NtQueryInformationToken(HANDLE token, TOKEN_INFORMATION_CLASS cls,
                                        PVOID info, ULONG length, PULONG ret);
NTSTATUS WINAPI RtlAdjustPrivilege(ULONG privilege, BOOLEAN enable, BOOLEAN current_thread, BOOLEAN *old);
NTSTATUS WINAPI NtShutdownSystem(ULONG action);
#define ShutdownReboot 1
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif
