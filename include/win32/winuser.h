#ifndef DISCOUNT_WINUSER_H
#define DISCOUNT_WINUSER_H

#include "windef.h"

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR   lpszMenuName;
    LPCWSTR   lpszClassName;
} WNDCLASSW, *PWNDCLASSW, *LPWNDCLASSW;

typedef struct tagWNDCLASSEXW {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR   lpszMenuName;
    LPCWSTR   lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXW, *PWNDCLASSEXW, *LPWNDCLASSEXW;

typedef struct tagNMHDR {
    HWND hwndFrom;
    UINT_PTR idFrom;
    UINT code;
} NMHDR, *LPNMHDR;

typedef struct tagWINDOWPLACEMENT {
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

typedef struct tagMINMAXINFO {
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

#define CW_USEDEFAULT ((int)0x80000000)

#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L

#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_ENABLE           0x000A
#define WM_SETREDRAW        0x000B
#define WM_SETTEXT          0x000C
#define WM_GETTEXT          0x000D
#define WM_GETTEXTLENGTH    0x000E
#define WM_GETICON          0x007F
#define WM_QUERYDRAGICON    0x0037
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUERYENDSESSION  0x0011
#define WM_QUIT             0x0012
#define WM_ERASEBKGND       0x0014
#define WM_SHOWWINDOW       0x0018
#define WM_SETCURSOR        0x0020
#define WM_GETMINMAXINFO    0x0024
#define WM_DRAWITEM         0x002B
#define WM_MEASUREITEM      0x002C
#define WM_COMMAND          0x0111
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115
#define WM_INITDIALOG       0x0110
#define WM_NOTIFY           0x004E
#define WM_USER             0x0400
#define WM_APP              0x8000
#define DM_SETDEFID         (WM_USER + 1)
#define WM_SETICON          0x0080
#define WM_NCPAINT          0x0085
#define WM_NCCREATE         0x0081
#define WM_NCCALCSIZE       0x0083
#define WM_ENTERMENULOOP    0x0211
#define WM_EXITMENULOOP     0x0212
#define WM_MENUSELECT       0x011F
#define WM_SIZING           0x0214
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_LBUTTONDBLCLK    0x0203
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MBUTTONDBLCLK    0x0209
#define WM_CAPTURECHANGED   0x0215
#define WM_MOUSEACTIVATE    0x0021
#define WM_MOUSEMOVE        0x0200
#define WM_CHAR             0x0102
#define WM_DEADCHAR         0x0103
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_GETHOTKEY        0x0033
#define WM_HOTKEY           0x0312
#define WM_SETHOTKEY        0x0032
#define WM_SYSCHAR          0x0106
#define WM_SYSDEADCHAR      0x0107
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_MOUSEHOVER       0x02A1
#define WM_MOUSELEAVE       0x02A3
#define WM_NCHITTEST        0x0084
#define WM_NCLBUTTONDOWN    0x00A1
#define WM_NCLBUTTONUP      0x00A2
#define WM_NCLBUTTONDBLCLK  0x00A3
#define WM_NCMBUTTONDOWN    0x00A7
#define WM_NCMBUTTONUP      0x00A8
#define WM_NCMBUTTONDBLCLK  0x00A9
#define WM_NCMOUSEMOVE      0x00A0
#define WM_NCRBUTTONDOWN    0x00A4
#define WM_NCRBUTTONUP      0x00A5
#define WM_NCRBUTTONDBLCLK  0x00A6

#define ICON_SMALL 0
#define ICON_BIG   1

#define MB_OK              0x00000000L
#define MB_YESNO           0x00000004L
#define MB_ICONSTOP        0x00000010L
#define MB_ICONHAND        MB_ICONSTOP
#define MB_ICONERROR       MB_ICONSTOP
#define MB_ICONWARNING     0x00000030L
#define MB_ICONINFORMATION 0x00000040L

#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDIGNORE 5

#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_SHOWMINIMIZED   2
#define SW_MAXIMIZE        3
#define SW_SHOWMAXIMIZED   3
#define SW_SHOWNOACTIVATE  4
#define SW_SHOW            5
#define SW_MINIMIZE        6
#define SW_RESTORE         9

#define VK_RETURN 0x0D

#define IMAGE_ICON 1

#define COLOR_WINDOW 5
#define COLOR_3DSHADOW 16
#define COLOR_3DHILIGHT 20

#define WS_EX_TOPMOST      0x00000008L
#define WS_EX_TOOLWINDOW   0x00000080L

#define GWL_STYLE          (-16)
#define GWL_EXSTYLE        (-20)
#define GWLP_WNDPROC       (-4)
#define GWLP_ID            (-12)
#define GCLP_HBRBACKGROUND (-10)
#define GCLP_HICON         (-14)
#define GCLP_HICONSM       (-34)

#define HWND_TOP           ((HWND)0)
#define HWND_TOPMOST       ((HWND)(LONG_PTR)-1)
#define HWND_NOTOPMOST     ((HWND)(LONG_PTR)-2)

#define SWP_NOSIZE         0x0001
#define SWP_NOMOVE         0x0002
#define SWP_NOZORDER       0x0004
#define SWP_NOACTIVATE     0x0010
#define SWP_SHOWWINDOW     0x0040
#define SWP_NOOWNERZORDER  0x0200

#define SIZE_MINIMIZED     1

#define SM_CXSMICON        49
#define SM_CYSMICON        50

#define LR_SHARED          0x00008000

#define HELP_FINDER        0x000b

#define TPM_LEFTBUTTON     0x0000L
#define TPM_LEFTALIGN      0x0000L
#define TPM_TOPALIGN       0x0000L

#define MDITILE_VERTICAL   0x0000
#define MDITILE_HORIZONTAL 0x0001

#define MF_BYCOMMAND       0x00000000L
#define MF_BYPOSITION      0x00000400L
#define MF_SEPARATOR       0x00000800L
#define MF_STRING          0x00000000L
#define MF_POPUP           0x00000010L
#define MF_CHECKED         0x00000008L
#define MF_UNCHECKED       0x00000000L
#define MF_DISABLED        0x00000002L
#define MF_GRAYED          0x00000001L
#define MF_ENABLED         0x00000000L

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define FORMAT_MESSAGE_FROM_STRING     0x00000400
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000

#define LANG_NEUTRAL       0x00

#define BST_CHECKED        1

#define BM_GETCHECK        0x00F0
#define BM_SETCHECK        0x00F1

#define DT_CENTER          0x00000001

#define WMSZ_LEFT          1
#define WMSZ_RIGHT         2
#define WMSZ_TOP           3
#define WMSZ_TOPLEFT       4
#define WMSZ_TOPRIGHT      5
#define WMSZ_BOTTOM        6
#define WMSZ_BOTTOMLEFT    7
#define WMSZ_BOTTOMRIGHT   8

ATOM WINAPI RegisterClassW(const WNDCLASSW *lpWndClass);
ATOM WINAPI RegisterClassExW(const WNDCLASSEXW *lpwcx);
LRESULT WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
LRESULT WINAPI DispatchMessageW(const MSG *lpMsg);
BOOL WINAPI TranslateMessage(const MSG *lpMsg);
int WINAPI ShowWindow(HWND hWnd, int nCmdShow);
BOOL WINAPI UpdateWindow(HWND hWnd);
void WINAPI PostQuitMessage(int nExitCode);
int WINAPI MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
int WINAPI MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
HWND WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                            DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                            HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
HWND WINAPI CreateDialogW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
INT_PTR WINAPI DialogBoxW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
LRESULT WINAPI SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI DestroyWindow(HWND hWnd);
BOOL WINAPI GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl);
BOOL WINAPI SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT *lpwndpl);
UINT WINAPI SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void *lpTimerFunc);
BOOL WINAPI KillTimer(HWND hWnd, UINT_PTR uIDEvent);
HICON WINAPI LoadIconW(HINSTANCE hInstance, LPCWSTR lpIconName);
HANDLE WINAPI LoadImageW(HINSTANCE hinst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad);
int WINAPI LoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, int cchBufferMax);
int WINAPI LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI GetWindowRect(HWND hWnd, LPRECT lpRect);
int WINAPI GetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount);
HWND WINAPI GetDlgItem(HWND hDlg, int nIDDlgItem);
BOOL WINAPI SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
int WINAPI GetSystemMetrics(int nIndex);
BOOL WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
HMENU WINAPI GetMenu(HWND hWnd);
HMENU WINAPI GetSubMenu(HMENU hMenu, int nPos);
int WINAPI GetMenuItemCount(HMENU hMenu);
BOOL WINAPI CheckMenuRadioItem(HMENU hmenu, UINT first, UINT last, UINT check, UINT flags);
BOOL WINAPI CheckMenuItem(HMENU hmenu, UINT idCheckItem, UINT uCheck);
BOOL WINAPI EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
BOOL WINAPI DestroyMenu(HMENU hMenu);
BOOL WINAPI RemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL WINAPI AppendMenuW(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);
HMENU WINAPI LoadMenuW(HINSTANCE hInstance, LPCWSTR lpMenuName);
BOOL WINAPI InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);
BOOL WINAPI DrawMenuBar(HWND hWnd);
BOOL WINAPI BringWindowToTop(HWND hWnd);
HWND WINAPI SetFocus(HWND hWnd);
HMENU WINAPI CreatePopupMenu(void);
DWORD WINAPI FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                            LPWSTR lpBuffer, DWORD nSize, void *Arguments);
HLOCAL WINAPI LocalFree(HLOCAL hMem);
BOOL WINAPI EndDialog(HWND hDlg, INT_PTR nResult);
BOOL WINAPI PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI WinHelpW(HWND hWndMain, LPCWSTR lpszHelp, UINT uCommand, ULONG_PTR dwData);
BOOL WINAPI GetCursorPos(LPPOINT lpPoint);
LONG_PTR WINAPI GetWindowLongW(HWND hWnd, int nIndex);
LONG WINAPI SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong);
LONG_PTR WINAPI GetWindowLongPtrW(HWND hWnd, int nIndex);
LONG_PTR WINAPI SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
LONG_PTR WINAPI GetClassLongPtrW(HWND hWnd, int nIndex);
BOOL WINAPI IsWindowVisible(HWND hWnd);
BOOL WINAPI IsIconic(HWND hWnd);
BOOL WINAPI DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL WINAPI SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos);
BOOL WINAPI TrackPopupMenuEx(HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, void *lptpm);
UINT WINAPI GetMenuState(HMENU hMenu, UINT uId, UINT uFlags);
HWND WINAPI GetWindow(HWND hWnd, UINT uCmd);
LRESULT WINAPI CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
HDC WINAPI GetDC(HWND hWnd);
int WINAPI ReleaseDC(HWND hWnd, HDC hDC);
DWORD WINAPI GetSysColor(int nIndex);
HDC WINAPI BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL WINAPI EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);
BOOL WINAPI OpenIcon(HWND hWnd);
BOOL WINAPI SetForegroundWindow(HWND hWnd);
int WINAPI wsprintfW(LPWSTR lpOut, LPCWSTR lpFmt, ...);
BOOL WINAPI EnableWindow(HWND hWnd, BOOL bEnable);
BOOL WINAPI CopyRect(LPRECT lprcDst, const RECT *lprcSrc);
int WINAPI DrawTextW(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format);
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
BOOL WINAPI IsWindow(HWND hWnd);
HWND WINAPI GetParent(HWND hWnd);
int WINAPI MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
BOOL WINAPI SetWindowTextW(HWND hWnd, LPCWSTR lpString);
BOOL WINAPI TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect);
LRESULT WINAPI SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam,
                                   UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
BOOL WINAPI EnumWindows(BOOL (CALLBACK *lpEnumFunc)(HWND, LPARAM), LPARAM lParam);
BOOL WINAPI IsHungAppWindow(HWND hWnd);
BOOL WINAPI DestroyIcon(HICON hIcon);
WORD WINAPI TileWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids);
WORD WINAPI CascadeWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids);
void WINAPI SwitchToThisWindow(HWND hWnd, BOOL fAltTab);
DWORD WINAPI GetWindowThreadProcessId(HWND hWnd, DWORD *lpdwProcessId);
BOOL WINAPI IsIconic(HWND hWnd);

#define GW_OWNER 4

#endif
