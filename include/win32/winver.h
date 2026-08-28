#ifndef DISCOUNT_WINVER_H
#define DISCOUNT_WINVER_H
#include "windows.h"
typedef struct {
    DWORD dwSignature, dwStrucVersion, dwFileVersionMS, dwFileVersionLS;
    DWORD dwProductVersionMS, dwProductVersionLS, dwFileFlagsMask, dwFileFlags;
    DWORD dwFileOS, dwFileType, dwFileSubtype, dwFileDateMS, dwFileDateLS;
} VS_FIXEDFILEINFO;
#define VS_FF_DEBUG 0x00000001
#define VS_FF_PRERELEASE 0x00000002
DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR filename, DWORD *handle);
BOOL WINAPI GetFileVersionInfoW(LPCWSTR filename, DWORD handle, DWORD length, LPVOID data);
BOOL WINAPI VerQueryValueW(const LPVOID block, LPCWSTR subblock, LPVOID *buffer, UINT *length);
#endif
