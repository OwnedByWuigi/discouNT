#ifndef DISCOUNT_MSGINA_WINNLS_H
#define DISCOUNT_MSGINA_WINNLS_H
#include "windows.h"
#define LOCALE_SYSTEM_DEFAULT 0x0800
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SLANGUAGE 0x00000002
int WINAPI GetLocaleInfoW(DWORD locale, DWORD type, LPWSTR data, int cch);
int WINAPI GetUserDefaultLocaleName(LPWSTR name, int cch);
#endif
