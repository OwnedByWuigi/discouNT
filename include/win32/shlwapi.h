#ifndef DISCOUNT_SHLWAPI_H
#define DISCOUNT_SHLWAPI_H

#include "windef.h"

BOOL WINAPI StrFormatByteSizeW(LONGLONG qdw, LPWSTR pszBuf, UINT cchBuf);
BOOL WINAPI StrFormatKBSizeW(LONGLONG qdw, LPWSTR pszBuf, UINT cchBuf);
int WINAPI StrCmpNW(LPCWSTR psz1, LPCWSTR psz2, int iLen);
int WINAPI StrCmpNIW(LPCWSTR psz1, LPCWSTR psz2, int iLen);
LPWSTR WINAPI StrRStrIW(LPCWSTR pszSource, LPCWSTR pszLast, LPCWSTR pszSrch);
LPWSTR WINAPI StrStrW(LPCWSTR pszFirst, LPCWSTR pszSrch);
LPWSTR WINAPI StrStrIW(LPCWSTR pszFirst, LPCWSTR pszSrch);
LPCWSTR WINAPI PathFindFileNameW(LPCWSTR path);
BOOL WINAPI PathRemoveBackslashW(LPWSTR path);
BOOL WINAPI PathIsDirectoryW(LPCWSTR path);

#endif
