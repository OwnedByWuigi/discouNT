#ifndef DISCOUNT_SHELLAPI_H
#define DISCOUNT_SHELLAPI_H

#include "winuser.h"

typedef struct _NOTIFYICONDATAW {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    WCHAR szTip[128];
    DWORD dwState,dwStateMask;
    WCHAR szInfo[256];
    union { UINT uTimeout,uVersion; };
    WCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
    HICON hBalloonIcon;
} NOTIFYICONDATAW, *PNOTIFYICONDATAW;
typedef struct _SHFILEINFOW { HICON hIcon; int iIcon; DWORD dwAttributes; WCHAR szDisplayName[MAX_PATH]; WCHAR szTypeName[80]; } SHFILEINFOW;

#define NIM_ADD    0x00000000
#define NIM_MODIFY 0x00000001
#define NIM_DELETE 0x00000002
#define NIM_SETVERSION 0x00000004

#define NIF_MESSAGE 0x00000001
#define NIF_ICON    0x00000002
#define NIF_TIP     0x00000004
#define NIF_STATE 0x00000008
#define NIF_INFO 0x00000010
#define NIS_HIDDEN 1
#define NIIF_ERROR 3
#define NIIF_USER 4
#define NIIF_ICONMASK 0x0f
#define NIIF_LARGEICON 0x20
#define NOTIFYICON_VERSION_4 4
#define NOTIFYICONDATAA_V2_SIZE 488
#define NIN_SELECT (WM_USER+0)
#define ABM_NEW 0
#define ABM_REMOVE 1
#define ABM_QUERYPOS 2
#define ABM_SETPOS 3
#define ABM_GETSTATE 4
#define ABM_GETTASKBARPOS 5
#define ABM_ACTIVATE 6
#define ABM_GETAUTOHIDEBAR 7
#define ABM_SETAUTOHIDEBAR 8
#define ABM_WINDOWPOSCHANGED 9
#define ABN_POSCHANGED 1
#define ABS_AUTOHIDE 1
#define ABS_ALWAYSONTOP 2
#define ABE_LEFT 0
#define ABE_TOP 1
#define ABE_RIGHT 2
#define ABE_BOTTOM 3
#define SEE_MASK_IDLIST 0x00000004
typedef struct _SHELLEXECUTEINFOW { DWORD cbSize; ULONG fMask; HWND hwnd; LPCWSTR lpVerb,lpFile,lpParameters,lpDirectory; INT nShow; HINSTANCE hInstApp; LPVOID lpIDList; LPCWSTR lpClass; HKEY hkeyClass; DWORD dwHotKey; union { HANDLE hIcon,hMonitor; }; HANDLE hProcess; } SHELLEXECUTEINFOW,*LPSHELLEXECUTEINFOW;
#define SHGFI_ICON 0x000000100
#define SHGFI_DISPLAYNAME 0x000000200
#define SHGFI_TYPENAME 0x000000400
#define SHGFI_ATTRIBUTES 0x000000800
#define SHGFI_SYSICONINDEX 0x000004000
#define SHGFI_SMALLICON 0x000000001
#define SHGFI_PIDL 0x000000008

BOOL WINAPI Shell_NotifyIconW(DWORD dwMessage, PNOTIFYICONDATAW lpData);
int WINAPI ShellAboutA(HWND hWnd, LPCSTR szApp, LPCSTR szOtherStuff, HICON hIcon);
int WINAPI ShellAboutW(HWND hWnd, LPCWSTR szApp, LPCWSTR szOtherStuff, HICON hIcon);
DWORD_PTR WINAPI SHGetFileInfoW(LPCWSTR path,DWORD attributes,SHFILEINFOW *info,UINT size,UINT flags);
HINSTANCE WINAPI ShellExecuteW(HWND hwnd,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show);
BOOL WINAPI ShellExecuteExW(LPSHELLEXECUTEINFOW info);

#endif
