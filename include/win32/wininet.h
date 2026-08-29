#ifndef DISCOUNT_WININET_H
#define DISCOUNT_WININET_H

#include <stdint.h>

typedef void *HINTERNET;
typedef uint32_t DWORD;
typedef int BOOL;
typedef const char *LPCSTR;
typedef char *LPSTR;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define INTERNET_OPEN_TYPE_DIRECT 1
#define INTERNET_SERVICE_HTTP 3
#define INTERNET_SERVICE_FTP 1

HINTERNET InternetOpenA(LPCSTR, DWORD, LPCSTR, LPCSTR, DWORD);
HINTERNET InternetOpenW(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
HINTERNET InternetOpenUrlA(HINTERNET, LPCSTR, LPCSTR, DWORD, DWORD, uintptr_t);
HINTERNET InternetOpenUrlW(HINTERNET, LPCWSTR, LPCWSTR, DWORD, DWORD, uintptr_t);
BOOL InternetReadFile(HINTERNET, void *, DWORD, DWORD *);
BOOL InternetCloseHandle(HINTERNET);
HINTERNET InternetConnectA(HINTERNET, LPCSTR, uint16_t, LPCSTR, LPCSTR, DWORD, DWORD, uintptr_t);
HINTERNET InternetConnectW(HINTERNET, LPCWSTR, uint16_t, LPCWSTR, LPCWSTR, DWORD, DWORD, uintptr_t);
HINTERNET HttpOpenRequestA(HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, const LPCSTR *, DWORD, uintptr_t);
HINTERNET HttpOpenRequestW(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, const LPCWSTR *, DWORD, uintptr_t);
BOOL HttpSendRequestA(HINTERNET, LPCSTR, DWORD, void *, DWORD);
BOOL HttpSendRequestW(HINTERNET, LPCWSTR, DWORD, void *, DWORD);
BOOL InternetQueryInfoA(HINTERNET, DWORD, void *, DWORD *, DWORD *);
BOOL InternetQueryInfoW(HINTERNET, DWORD, void *, DWORD *, DWORD *);

#endif
