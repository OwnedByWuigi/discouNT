#ifndef DISCOUNT_MSGINA_POFUNCS_H
#define DISCOUNT_MSGINA_POFUNCS_H
#include "winnt.h"
NTSTATUS WINAPI NtSetSystemPowerState(ULONG action, ULONG min_state, ULONG flags);
#endif
