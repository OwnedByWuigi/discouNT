#ifndef DISCOUNT_SHLWAPI_H
#define DISCOUNT_SHLWAPI_H

#include "windef.h"

BOOL WINAPI StrFormatByteSizeW(LONGLONG qdw, LPWSTR pszBuf, UINT cchBuf);
BOOL WINAPI StrFormatKBSizeW(LONGLONG qdw, LPWSTR pszBuf, UINT cchBuf);

#endif
