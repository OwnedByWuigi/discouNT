#ifndef DISCOUNT_WINDEF_H
#define DISCOUNT_WINDEF_H

#include <stdint.h>
#include <stddef.h>
void *memcpy(void *dst, const void *src, uint32_t bytes);

typedef int BOOL;
typedef uint8_t BYTE;
typedef BYTE *PBYTE;
typedef uint16_t WORD;
typedef int16_t SHORT;
typedef uint8_t BOOLEAN;
typedef uint32_t DWORD;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int INT;
typedef INT *LPINT;
typedef uint32_t UINT;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef DWORD_PTR *PDWORD_PTR;
typedef uintptr_t SIZE_T;
typedef intptr_t INT_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef void *HANDLE;
typedef void *PVOID;
typedef void VOID;
typedef HANDLE HWND;
typedef HANDLE HDESK;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HACCEL;
typedef HANDLE HBRUSH;
typedef HANDLE HPEN;
typedef HANDLE HFONT;
typedef HANDLE HMENU;
typedef HANDLE HDC;
typedef HANDLE HBITMAP;
typedef HANDLE HIMAGELIST;
typedef HANDLE HTOKEN;
typedef HANDLE HGLOBAL;
typedef HANDLE HGDIOBJ;
typedef HANDLE HLOCAL;
typedef HANDLE HKEY;
typedef WORD ATOM;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef const wchar_t *LPCWSTR;
typedef wchar_t *LPWSTR;
typedef const void *LPCVOID;
typedef void *LPVOID;
typedef ULONG *PULONG;
typedef DWORD *PDWORD;
typedef HANDLE *PHANDLE;
typedef BYTE *LPBYTE;
typedef wchar_t WCHAR;
typedef WCHAR *PWSTR;
typedef const WCHAR *PCWSTR;
typedef LONG HRESULT;
typedef void *FARPROC;
#ifndef __cdecl
#define __cdecl
#endif
typedef void *LPUNKNOWN;
typedef WCHAR *BSTR;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT, *LPCRECT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagFILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct tagSYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define TRUE 1
#define FALSE 0

#define CALLBACK
#define WINAPI
#define APIENTRY WINAPI
#define NTAPI WINAPI
#define PASCAL WINAPI
#define CDECL
#define STDAPICALLTYPE WINAPI
#define IN
#define OUT
#define OPTIONAL
#define _In_
#define _In_opt_
#define _Inout_
#define _Out_
#define _Out_opt_
#define _Inout_opt_
#define _Outptr_
#define _Outptr_result_maybenull_
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define C_ASSERT(x) typedef char __C_ASSERT__[(x) ? 1 : -1]
#define _countof(a) (sizeof(a) / sizeof((a)[0]))

#define MAX_PATH 260

#define LOWORD(l)   ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#define HIWORD(l)   ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w)   ((BYTE)((DWORD_PTR)(w) & 0xFF))
#define HIBYTE(w)   ((BYTE)((DWORD_PTR)(w) >> 8))
#define MAKELONG(a,b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define MAKEWORD(a,b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELPARAM(l,h) ((LPARAM)(DWORD)MAKELONG(l,h))
#define MAKEWPARAM(l,h) ((WPARAM)(DWORD)MAKELONG(l,h))
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#endif
