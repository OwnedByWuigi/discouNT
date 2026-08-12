#ifndef DISCOUNT_MSGINA_SEFUNCS_H
#define DISCOUNT_MSGINA_SEFUNCS_H
#include "winnt.h"
NTSTATUS WINAPI NtSetInformationToken(HANDLE token, ULONG cls, PVOID info, ULONG len);
#endif
