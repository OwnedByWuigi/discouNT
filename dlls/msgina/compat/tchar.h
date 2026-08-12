#ifndef DISCOUNT_MSGINA_TCHAR_H
#define DISCOUNT_MSGINA_TCHAR_H
#include <stdint.h>
#include "windef.h"
typedef char TCHAR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
#define _T(x) x
#define TEXT(x) x
#define PostMessage PostMessageW
#define SetWindowText SetWindowTextW
#define GetWindowTextLength GetWindowTextLengthW
#define GetObject GetObjectW
WCHAR *wcschr(const WCHAR *s, WCHAR c);
WCHAR *wcscat(WCHAR *d, const WCHAR *s);
#endif
