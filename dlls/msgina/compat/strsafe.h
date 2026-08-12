#ifndef DISCOUNT_MSGINA_STRSAFE_H
#define DISCOUNT_MSGINA_STRSAFE_H
#include "windows.h"
#define STRSAFE_NO_TRUNCATION 0x00000001
#define STRSAFE_NULL_ON_FAILURE 0x00000002
HRESULT WINAPI StringCbCopyNExW(LPWSTR dst, SIZE_T dst_bytes, LPCWSTR src,
                                SIZE_T src_bytes, LPWSTR *end, SIZE_T *remaining,
                                DWORD flags);
HRESULT WINAPI StringCbPrintfW(LPWSTR dst, SIZE_T dst_bytes, LPCWSTR format, ...);
#endif
