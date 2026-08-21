#ifndef DISCOUNT_WINUSER_H
#define DISCOUNT_WINUSER_H

#include "windef.h"
typedef struct _BLENDFUNCTION BLENDFUNCTION;
typedef struct _USEROBJECTFLAGS { BOOL fInherit,fReserved; DWORD dwFlags; } USEROBJECTFLAGS,*PUSEROBJECTFLAGS;
typedef struct _DEVMODEW { WCHAR dmDeviceName[32]; WORD dmSpecVersion,dmDriverVersion,dmSize,dmDriverExtra; DWORD dmFields; struct { POINT dmPosition; DWORD dmDisplayOrientation,dmDisplayFixedOutput; }; WCHAR dmFormName[32]; WORD dmLogPixels; DWORD dmBitsPerPel,dmPelsWidth,dmPelsHeight,dmDisplayFlags,dmDisplayFrequency; } DEVMODEW,*LPDEVMODEW;
typedef struct _DISPLAY_DEVICEW { DWORD cb; WCHAR DeviceName[32],DeviceString[128]; DWORD StateFlags; WCHAR DeviceID[128],DeviceKey[128]; } DISPLAY_DEVICEW,*PDISPLAY_DEVICEW;

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
typedef struct tagCREATESTRUCTW { LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent; int cy,cx,y,x; LONG style; LPCWSTR lpszName,lpszClass; DWORD dwExStyle; } CREATESTRUCTW,*LPCREATESTRUCTW;
typedef struct tagWINDOWPOS { HWND hwnd,hwndInsertAfter; int x,y,cx,cy; UINT flags; } WINDOWPOS,*LPWINDOWPOS;
typedef struct tagDRAWITEMSTRUCT { UINT CtlType; UINT CtlID; UINT itemID; UINT itemAction; UINT itemState; HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData; } DRAWITEMSTRUCT, *PDRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;
typedef struct tagCOPYDATASTRUCT { ULONG_PTR dwData; DWORD cbData; LPVOID lpData; } COPYDATASTRUCT, *PCOPYDATASTRUCT;
typedef struct tagTPMPARAMS { UINT cbSize; RECT rcExclude; } TPMPARAMS,*LPTPMPARAMS;
typedef struct tagMENUITEMINFOW { UINT cbSize,fMask,fType,fState,wID; HMENU hSubMenu; HBITMAP hbmpChecked,hbmpUnchecked; ULONG_PTR dwItemData; LPWSTR dwTypeData; UINT cch; HBITMAP hbmpItem; } MENUITEMINFOW,*LPMENUITEMINFOW;
typedef struct tagMENUINFO { DWORD cbSize,fMask,dwStyle; UINT cyMax; HBRUSH hbrBack; DWORD dwContextHelpID; ULONG_PTR dwMenuData; } MENUINFO,*LPMENUINFO;
typedef struct tagMEASUREITEMSTRUCT { UINT CtlType,CtlID,itemID,itemWidth,itemHeight; ULONG_PTR itemData; } MEASUREITEMSTRUCT,*LPMEASUREITEMSTRUCT;

#define CW_USEDEFAULT ((int)0x80000000)
#define MAKEINTATOM(i) ((LPCWSTR)(ULONG_PTR)(WORD)(i))
#define HWND_MESSAGE ((HWND)(LONG_PTR)-3)

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

/* Scroll-bar commands and SCROLLINFO flags. */
#define SB_HORZ             0
#define SB_VERT             1
#define SB_CTL              2
#define SB_BOTH             3
#define SB_LINEUP           0
#define SB_LINELEFT         0
#define SB_LINEDOWN         1
#define SB_LINERIGHT        1
#define SB_PAGEUP           2
#define SB_PAGELEFT         2
#define SB_PAGEDOWN         3
#define SB_PAGERIGHT        3
#define SB_THUMBPOSITION    4
#define SB_THUMBTRACK       5
#define SB_TOP               6
#define SB_LEFT              6
#define SB_BOTTOM            7
#define SB_RIGHT             7
#define SB_ENDSCROLL        8
#define SIF_RANGE            0x0001
#define SIF_PAGE             0x0002
#define SIF_POS              0x0004
#define SIF_DISABLENOSCROLL  0x0008
#define SIF_TRACKPOS         0x0010
#define SIF_ALL              (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)

typedef struct tagSCROLLINFO {
    UINT cbSize;
    UINT fMask;
    int nMin;
    int nMax;
    UINT nPage;
    int nPos;
    int nTrackPos;
} SCROLLINFO, *LPSCROLLINFO, *LPCSCROLLINFO;

#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_ENABLE           0x000A
#define WM_SETREDRAW        0x000B
#define WM_SETTEXT          0x000C
#define WM_GETTEXT          0x000D
#define WM_GETTEXTLENGTH    0x000E
#define WM_SETFONT          0x0030
#define WM_GETFONT          0x0031
#define WM_GETICON          0x007F
#define WM_GETDLGCODE       0x0087
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
#define WM_CUT              0x0300
#define WM_COPY             0x0301
#define WM_PASTE            0x0302
#define WM_CLEAR            0x0303
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115
#define WM_INITDIALOG       0x0110
#define WM_NEXTDLGCTL       0x0028
#define WM_NOTIFY           0x004E
#define WM_COPYDATA         0x004A
#define WM_APPCOMMAND       0x0319
#define WM_MENUCOMMAND      0x0126
#define WM_CONTEXTMENU      0x007b
#define WM_SETTINGCHANGE    0x001a
#define WM_DISPLAYCHANGE    0x007e
#define WM_PARENTNOTIFY     0x0210
#define WM_RENDERFORMAT     0x0305
#define WM_CLIPBOARDUPDATE  0x031d
#define WM_USER             0x0400
#define WM_APP              0x8000
#define WM_DROPFILES        0x0233
#define DM_SETDEFID         (WM_USER + 1)
#define WM_SETICON          0x0080
#define WM_NCPAINT          0x0085
#define WM_NCCREATE         0x0081
#define WM_NCCALCSIZE       0x0083
#define WM_ENTERMENULOOP    0x0211
#define WM_EXITMENULOOP     0x0212
#define WM_INITMENUPOPUP    0x0117
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
#define WM_MOUSEWHEEL       0x020A
#define WM_CAPTURECHANGED   0x0215
#define WM_DESTROYCLIPBOARD 0x0307
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
#define MB_OKCANCEL        0x00000001L
#define MB_YESNO           0x00000004L
#define MB_SYSTEMMODAL     0x00001000L
#define MB_YESNOCANCEL     0x00000003L
#define MB_ICONSTOP        0x00000010L
#define MB_ICONHAND        MB_ICONSTOP
#define MB_ICONERROR       MB_ICONSTOP
#define MB_ICONQUESTION    0x00000020L
#define MB_ICONWARNING     0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_ICONEXCLAMATION MB_ICONWARNING
#define MB_ICONMASK        0x000000F0L

#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#define IDIGNORE 5
#define IDHELP 9
#define BM_CLICK 0x00F5
#define CB_RESETCONTENT 0x014B
#define CB_FINDSTRINGEXACT 0x0158
#define CB_GETITEMDATA 0x0150
#define EM_GETLINE 0x00C4
#define BN_CLICKED 0
#define LANG_CHINESE 0x04
#define LANG_JAPANESE 0x11
#define LANG_KOREAN 0x12
#define PRIMARYLANGID(lid) ((WORD)(lid) & 0x3ff)

#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_SHOWMINIMIZED   2
#define SW_MAXIMIZE        3
#define SW_SHOWMAXIMIZED   3
#define SW_SHOWNOACTIVATE  4
#define SW_SHOW            5
#define SW_MINIMIZE        6
#define SW_RESTORE         9
#define SW_SHOWDEFAULT     10

#define VK_RETURN 0x0D
#define VK_TAB    0x09
#define VK_ESCAPE 0x1B

#define DLGC_WANTARROWS      0x0001
#define DLGC_WANTTAB         0x0002
#define DLGC_WANTALLKEYS     0x0004
#define DLGC_HASSETSEL       0x0008
#define DLGC_DEFPUSHBUTTON   0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON     0x0040
#define DLGC_WANTCHARS       0x0080
#define DLGC_STATIC          0x0100
#define DLGC_BUTTON          0x2000
#define MK_LBUTTON 0x0001

#define IMAGE_ICON 1

#define COLOR_WINDOW 5
#define COLOR_BACKGROUND 1
#define COLOR_3DSHADOW 16
#define COLOR_3DHILIGHT 20
#define APPCOMMAND_BROWSER_BACKWARD 1
#define APPCOMMAND_BROWSER_FORWARD 2
#define GET_APPCOMMAND_LPARAM(lParam) ((short)HIWORD(lParam)&~0xf000)
#define MIIM_STATE 1
#define MIIM_ID 2
#define MIIM_SUBMENU 4
#define MIIM_DATA 0x20
#define MIIM_STRING 0x40
#define MIIM_BITMAP 0x80
#define MIIM_FTYPE 0x100
#define MIM_MENUDATA 8
#define MIM_STYLE 0x10
#define MNS_NOTIFYBYPOS 0x08000000
#define MNS_NOCHECK 0x80000000
#define MFT_SEPARATOR 0x800
#define HBMMENU_CALLBACK ((HBITMAP)(LONG_PTR)-1)
#define TPM_BOTTOMALIGN 0x20
#define TPM_VERTICAL 0x40
#define ODT_MENU 1
#define MA_NOACTIVATE 3
#define CS_VREDRAW 1
#define CS_HREDRAW 2
#define CS_DBLCLKS 8
#define WS_EX_LAYERED 0x00080000
#define WS_EX_NOACTIVATE 0x08000000
#define ULW_ALPHA 2
#define DI_MASK 1
#define DI_NORMAL 3
#define DI_DEFAULTSIZE 8
#define SWP_HIDEWINDOW 0x0080
#define GA_ROOTOWNER 3
#define WC_BUTTONW L"Button"
#define BS_OWNERDRAW 0x0000000b
#define SPI_GETWORKAREA 0x0030
#define SPI_GETNONCLIENTMETRICS 0x0029
#define SPI_GETICONTITLELOGFONT 0x001f
#define SPI_SETDESKWALLPAPER 0x0014
#define DFC_BUTTON 4
#define DFCS_BUTTONPUSH 0x10
#define DFCS_PUSHED 0x200
#define DFCS_ADJUSTRECT 0x2000
#define ODS_SELECTED 1
#define DC_ACTIVE 1
#define DC_TEXT 8
#define DC_ICON 4
#define DC_INBUTTON 0x10
#define SC_MINIMIZE 0xf020
#define SC_RESTORE 0xf120
#define SC_CLOSE 0xf060
#define HTCLIENT 1
#define GW_CHILD 5
#define GW_HWNDNEXT 2
#define IDI_WINLOGO ((LPCWSTR)32517)
#define OIC_WINLOGO 32517
#define DT_WORDBREAK 0x10
#define DT_EDITCONTROL 0x2000
#define DT_END_ELLIPSIS 0x8000
#define PM_REMOVE 1
#define QS_ALLINPUT 0x04ff
#define UOI_FLAGS 1
#define UOI_NAME 2
#define WSF_VISIBLE 1
#define DF_WINE_ROOT_DESKTOP 0x40000000
#define DF_WINE_VIRTUAL_DESKTOP 0x80000000
#define SM_CXICON 11
#define SM_CYICON 12
#define SM_XVIRTUALSCREEN 76
#define SM_YVIRTUALSCREEN 77
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
#define DM_POSITION 0x20
#define DM_PELSWIDTH 0x00080000
#define DM_PELSHEIGHT 0x00100000
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define CDS_UPDATEREGISTRY 1
#define CDS_GLOBAL 8
#define CDS_NORESET 0x10000000
BOOL WINAPI AdjustWindowRect(LPRECT rect,DWORD style,BOOL menu);
BOOL WINAPI DrawIconEx(HDC dc,int x,int y,HICON icon,int cx,int cy,UINT step,HBRUSH brush,UINT flags);
BOOL WINAPI UpdateLayeredWindow(HWND hwnd,HDC dst,const POINT *dst_pt,const SIZE *size,HDC src,const POINT *src_pt,COLORREF key,const BLENDFUNCTION *blend,DWORD flags);
BOOL WINAPI SendNotifyMessageW(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);
HWND WINAPI SetParent(HWND child,HWND parent);
HICON WINAPI CopyIcon(HICON icon);
HWND WINAPI GetAncestor(HWND hwnd,UINT flags);
HWND WINAPI GetForegroundWindow(void);
HWND WINAPI GetActiveWindow(void);
BOOL WINAPI SystemParametersInfoW(UINT action,UINT param,PVOID data,UINT flags);
HICON WINAPI CreateIcon(HINSTANCE instance,int width,int height,BYTE planes,BYTE bpp,const BYTE *and_bits,const BYTE *xor_bits);
BOOL WINAPI DrawFrameControl(HDC dc,LPRECT rect,UINT type,UINT state);
BOOL WINAPI DrawCaptionTempW(HWND hwnd,HDC dc,const RECT *rect,HFONT font,HICON icon,LPCWSTR text,UINT flags);
BOOL WINAPI IsWindowEnabled(HWND hwnd);
BOOL WINAPI IntersectRect(LPRECT out,const RECT *a,const RECT *b);
UINT WINAPI ExtractIconExW(LPCWSTR file,int index,HICON *large,HICON *small,UINT count);
BOOL WINAPI GetUserObjectInformationW(HANDLE object,int index,PVOID info,DWORD length,DWORD *needed);
HWINSTA WINAPI GetProcessWindowStation(void);
BOOL WINAPI PeekMessageW(LPMSG msg,HWND hwnd,UINT min,UINT max,UINT remove);
DWORD WINAPI MsgWaitForMultipleObjects(DWORD count,const HANDLE *handles,BOOL all,DWORD timeout,DWORD mask);
LONG WINAPI ChangeDisplaySettingsExW(LPCWSTR device,DEVMODEW *mode,HWND hwnd,DWORD flags,LPVOID param);
BOOL WINAPI EnumDisplayDevicesW(LPCWSTR device,DWORD index,PDISPLAY_DEVICEW display,DWORD flags);
BOOL WINAPI EnumDisplaySettingsExW(LPCWSTR device,DWORD mode,LPDEVMODEW settings,DWORD flags);
HCURSOR WINAPI SetCursor(HCURSOR cursor);
HCURSOR WINAPI LoadCursorA(HINSTANCE instance,LPCSTR cursor);
BOOL WINAPI PaintDesktop(HDC dc);
HDESK WINAPI GetThreadDesktop(DWORD thread);
BOOL WINAPI SetShellWindow(HWND shell);
HDESK WINAPI CreateDesktopW(LPCWSTR desktop,LPCWSTR device,DEVMODEW *mode,DWORD flags,DWORD access,void *attributes);
BOOL WINAPI ClipCursor(const RECT *rect);
BOOL WINAPI GetMenuItemInfoW(HMENU menu,UINT item,BOOL by_position,LPMENUITEMINFOW info);
BOOL WINAPI SetMenuItemInfoW(HMENU menu,UINT item,BOOL by_position,const MENUITEMINFOW *info);
BOOL WINAPI InsertMenuItemW(HMENU menu,UINT item,BOOL by_position,const MENUITEMINFOW *info);
BOOL WINAPI GetMenuInfo(HMENU menu,LPMENUINFO info);
BOOL WINAPI SetMenuInfo(HMENU menu,const MENUINFO *info);

#define WS_EX_TOPMOST      0x00000008L
#define WS_EX_TOOLWINDOW   0x00000080L
#define WS_EX_CLIENTEDGE   0x00000200L

#define GWL_STYLE          (-16)
#define GWL_EXSTYLE        (-20)
#define GWLP_WNDPROC       (-4)
#define GWLP_ID            (-12)
#define GCLP_HBRBACKGROUND (-10)
#define GCLP_HCURSOR       (-12)
#define GCLP_HICON         (-14)
#define GCLP_HMODULE       (-16)
#define GCLP_WNDPROC       (-24)
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
#define SIZE_RESTORED      0

#define SM_CXSCREEN        0
#define SM_CYSCREEN        1
#define SM_CYCAPTION       4
#define SM_CXBORDER        5
#define SM_CYBORDER        6
#define SM_CYMENU          15
#define SM_CXFRAME         32
#define SM_CYFRAME         33
#define SM_CXSMICON        49
#define SM_CYSMICON        50

#define ES_LEFT            0x0000L
#define ES_MULTILINE       0x0004L
#define ES_PASSWORD        0x0020L
#define ES_AUTOVSCROLL     0x0040L
#define ES_AUTOHSCROLL     0x0080L
#define ES_NOHIDESEL       0x0100L

#define BS_PUSHBUTTON      0x00000000L
#define BS_DEFPUSHBUTTON   0x00000001L
#define BS_CHECKBOX        0x00000002L
#define BS_AUTOCHECKBOX    0x00000003L
#define BS_GROUPBOX        0x00000007L

#define CBS_SIMPLE         0x0001L
#define CBS_DROPDOWN       0x0002L
#define CBS_DROPDOWNLIST   0x0003L

#define EM_GETSEL          0x00B0
#define EM_SETSEL          0x00B1
#define EM_GETMODIFY       0x00B8
#define EM_SETMODIFY       0x00B9
#define EM_LINEINDEX       0x00BB
#define EM_REPLACESEL      0x00C2
#define EM_LIMITTEXT       0x00C5
#define EM_CANUNDO         0x00C6
#define EM_UNDO            0x00C7
#define EM_LINEFROMCHAR    0x00C9
#define EM_EMPTYUNDOBUFFER 0x00CD

#define CF_TEXT            1

#define LR_SHARED          0x00008000
#define IDC_ARROW          ((LPWSTR)((ULONG_PTR)32512))

#define HELP_FINDER        0x000b
#define HELP_INDEX         0x0003

#define TPM_LEFTBUTTON     0x0000L
#define TPM_LEFTALIGN      0x0000L
#define TPM_TOPALIGN       0x0000L

#define MDITILE_VERTICAL   0x0000
#define MDITILE_HORIZONTAL 0x0001

#define CBN_SELCHANGE      1
#define CB_ERR             (-1)
#define CB_ADDSTRING       0x0143
#define CB_GETCURSEL       0x0147
#define CB_SETCURSEL       0x014E

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

#define BST_UNCHECKED      0
#define BST_CHECKED        1

#define BM_GETCHECK        0x00F0
#define BM_SETCHECK        0x00F1
#define BM_CLICK           0x00F5

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
BOOL WINAPI UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance);
int WINAPI GetClassNameW(HWND hWnd, LPWSTR lpClassName, int nMaxCount);
BOOL WINAPI GetClassInfoW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSW lpWndClass);
BOOL WINAPI GetClassInfoExW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSEXW lpwcx);
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
HWND WINAPI CreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
HWND WINAPI CreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
INT_PTR WINAPI DialogBoxW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
INT_PTR WINAPI DialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
INT_PTR WINAPI DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
HWND WINAPI CreateDialogIndirectParamW(HINSTANCE hInstance, LPCVOID lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
INT_PTR WINAPI DialogBoxIndirectParamW(HINSTANCE hInstance, LPCVOID lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
LRESULT WINAPI DefDlgProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
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
int WINAPI GetScrollPos(HWND hWnd, int nBar);
BOOL WINAPI GetScrollInfo(HWND hWnd, int nBar, LPSCROLLINFO lpScrollInfo);
int WINAPI SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw);
int WINAPI SetScrollInfo(HWND hWnd, int nBar, LPCSCROLLINFO lpScrollInfo, BOOL bRedraw);
BOOL WINAPI ShowScrollBar(HWND hWnd, int wBar, BOOL bShow);
int WINAPI GetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount);
int WINAPI GetWindowTextLengthW(HWND hWnd);
HWND WINAPI GetDlgItem(HWND hDlg, int nIDDlgItem);
UINT WINAPI GetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPWSTR lpString, int cchMax);
LRESULT WINAPI SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString);
UINT WINAPI GetDlgItemInt(HWND hDlg, int nIDDlgItem, BOOL *lpTranslated, BOOL bSigned);
BOOL WINAPI SetDlgItemInt(HWND hDlg, int nIDDlgItem, UINT uValue, BOOL bSigned);
BOOL WINAPI CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck);
UINT WINAPI IsDlgButtonChecked(HWND hDlg, int nIDButton);
BOOL WINAPI CheckRadioButton(HWND hDlg, int firstID, int lastID, int checkID);
BOOL WINAPI SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
BOOL WINAPI EqualRect(const RECT *first,const RECT *second);
BOOL WINAPI SubtractRect(LPRECT result,const RECT *first,const RECT *second);
BOOL WINAPI ExitWindows(DWORD reserved,UINT reason);
int WINAPI GetSystemMetrics(int nIndex);
BOOL WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
HMENU WINAPI GetMenu(HWND hWnd);
BOOL WINAPI SetMenu(HWND hWnd, HMENU hMenu);
HMENU WINAPI CreateMenu(void);
HMENU WINAPI GetSubMenu(HMENU hMenu, int nPos);
int WINAPI GetMenuItemCount(HMENU hMenu);
BOOL WINAPI CheckMenuRadioItem(HMENU hmenu, UINT first, UINT last, UINT check, UINT flags);
BOOL WINAPI CheckMenuItem(HMENU hmenu, UINT idCheckItem, UINT uCheck);
BOOL WINAPI EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
BOOL WINAPI DestroyMenu(HMENU hMenu);
BOOL WINAPI RemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL WINAPI AppendMenuW(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);
BOOL WINAPI AppendMenuA(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem);
HMENU WINAPI LoadMenuW(HINSTANCE hInstance, LPCWSTR lpMenuName);
HMENU WINAPI LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName);
BOOL WINAPI InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem);
BOOL WINAPI InsertMenuA(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem);
BOOL WINAPI DrawMenuBar(HWND hWnd);
BOOL WINAPI BringWindowToTop(HWND hWnd);
HWND WINAPI SetFocus(HWND hWnd);
HWND WINAPI SetActiveWindow(HWND hWnd);
HWND WINAPI GetDesktopWindow(void);
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
ULONG_PTR WINAPI SetClassLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
BOOL WINAPI IsWindowVisible(HWND hWnd);
BOOL WINAPI IsIconic(HWND hWnd);
BOOL WINAPI DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL WINAPI SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos);
BOOL WINAPI TrackPopupMenuEx(HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, void *lptpm);
UINT WINAPI GetMenuState(HMENU hMenu, UINT uId, UINT uFlags);
UINT WINAPI GetMenuItemID(HMENU hMenu, int nPos);
BOOL WINAPI IsMenu(HMENU hMenu);
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
BOOL WINAPI IsClipboardFormatAvailable(UINT format);
BOOL WINAPI CopyRect(LPRECT lprcDst, const RECT *lprcSrc);
int WINAPI DrawTextW(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format);
BOOL WINAPI InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
BOOL WINAPI IsWindow(HWND hWnd);
HWND WINAPI GetParent(HWND hWnd);
int WINAPI MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI MapDialogRect(HWND hDlg, LPRECT lpRect);
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
HCURSOR WINAPI LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName);
UINT WINAPI RegisterWindowMessageW(LPCWSTR lpString);
BOOL WINAPI DragAcceptFiles(HWND hWnd, BOOL fAccept);
UINT WINAPI DragQueryFileW(HANDLE hDrop, UINT iFile, LPWSTR lpszFile, UINT cch);
void WINAPI DragFinish(HANDLE hDrop);
HWND WINAPI CreateWindowW(LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
                          int X, int Y, int nWidth, int nHeight, HWND hWndParent,
                          HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

#define GW_OWNER 4

#endif
