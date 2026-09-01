#ifndef DISCOUNT_STRSAFE_H
#define DISCOUNT_STRSAFE_H
#include <windows.h>
#define StringCchCopyW(dst,n,src) (lstrcpynW((dst),(src),(n)), S_OK)
#define StringCbCopyW(dst,n,src) (lstrcpynW((dst),(src),(n)/sizeof(WCHAR)), S_OK)
#define StringCbCopyNW(dst,n,src,cb) (lstrcpynW((dst),(src),(n)/sizeof(WCHAR)), S_OK)
#define StringCbCatW(dst,n,src) (lstrcatW((dst),(src)), S_OK)
#define STRSAFE_NO_TRUNCATION 0x00000001
#define STRSAFE_NULL_ON_FAILURE 0x00000002
HRESULT WINAPI StringCbCopyNExW(LPWSTR dst, SIZE_T cb, LPCWSTR src, SIZE_T src_cb, LPWSTR *end, SIZE_T *remaining, DWORD flags);
HRESULT WINAPI StringCbPrintfW(LPWSTR dst, SIZE_T cb, LPCWSTR fmt, ...);
#endif
