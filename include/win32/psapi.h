#ifndef DISCOUNT_PSAPI_H
#define DISCOUNT_PSAPI_H
#include "windows.h"
BOOL WINAPI EnumProcesses(DWORD *processes, DWORD bytes, DWORD *needed);
BOOL WINAPI EnumProcessModules(HANDLE process, HMODULE *modules, DWORD bytes, DWORD *needed);
DWORD WINAPI GetModuleBaseNameW(HANDLE process, HMODULE module, LPWSTR name, DWORD size);
#endif
