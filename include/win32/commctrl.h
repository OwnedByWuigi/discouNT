#ifndef DISCOUNT_COMMCTRL_H
#define DISCOUNT_COMMCTRL_H

#include "winuser.h"

typedef LRESULT (CALLBACK *SUBCLASSPROC)(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

typedef struct tagINITCOMMONCONTROLSEX {
    DWORD dwSize;
    DWORD dwICC;
} INITCOMMONCONTROLSEX, *LPINITCOMMONCONTROLSEX;

typedef struct tagTCITEMW {
    UINT mask;
    DWORD dwState;
    DWORD dwStateMask;
    LPWSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} TCITEMW, *LPTCITEMW;

typedef struct tagLVCOLUMNW {
    UINT mask;
    int fmt;
    int cx;
    LPWSTR pszText;
    int cchTextMax;
    int iSubItem;
    int iImage;
    int iOrder;
} LVCOLUMNW, *LPLVCOLUMNW;

typedef struct tagLVITEMW {
    UINT mask;
    int iItem;
    int iSubItem;
    UINT state;
    UINT stateMask;
    LPWSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
    int iIndent;
} LVITEMW, *LPLVITEMW;
typedef LVITEMW LV_ITEMW;

typedef struct tagHDITEMW {
    UINT mask;
    int cxy;
    LPWSTR pszText;
    HBITMAP hbm;
    int cchTextMax;
    int fmt;
    LPARAM lParam;
    int iImage;
    int iOrder;
} HDITEMW, *LPHDITEMW;

typedef struct tagLVHITTESTINFO {
    POINT pt;
    UINT flags;
    int iItem;
    int iSubItem;
} LVHITTESTINFO, *LPLVHITTESTINFO;

typedef struct tagNMITEMACTIVATE {
    NMHDR hdr;
    int iItem;
    int iSubItem;
    UINT uNewState;
    UINT uOldState;
    UINT uChanged;
    POINT ptAction;
    LPARAM lParam;
    UINT uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;

typedef struct tagNMLVDISPINFOW {
    NMHDR hdr;
    LVITEMW item;
} NMLVDISPINFOW, *LPNMLVDISPINFOW;

typedef NMLVDISPINFOW LV_DISPINFOW;

#define ICC_LISTVIEW_CLASSES  0x00000001
#define ICC_BAR_CLASSES       0x00000004
#define ICC_TAB_CLASSES       0x00000008

#define LVS_ICON              0x0000
#define LVS_REPORT            0x0001
#define LVS_SMALLICON         0x0002
#define LVS_LIST              0x0003

#define LVS_EX_HEADERDRAGDROP 0x00000010
#define LVS_EX_FULLROWSELECT  0x00000020

#define LVSIL_NORMAL          0
#define LVSIL_SMALL           1

#define LVIF_TEXT             0x0001
#define LVIF_IMAGE            0x0002
#define LVIF_PARAM            0x0004
#define LVIF_STATE            0x0008

#define LVCF_FMT              0x0001
#define LVCF_WIDTH            0x0002
#define LVCF_TEXT             0x0004
#define LVCF_SUBITEM          0x0008
#define LVCF_ORDER            0x0020

#define LVCFMT_LEFT           0x0000
#define LVCFMT_RIGHT          0x0001
#define LVCFMT_CENTER         0x0002

#define HDI_WIDTH             0x0001
#define HDI_TEXT              0x0002
#define HDI_FORMAT            0x0004
#define HDI_LPARAM            0x0008
#define HDI_ORDER             0x0080

#define SBT_NOBORDERS         0x0100
#define STATUSCLASSNAMEW      L"msctls_statusbar32"

#define SB_SETPARTS           (WM_USER + 4)
#define SB_SETTEXTW           (WM_USER + 11)

#define TCM_FIRST             0x1300

#define TCN_FIRST             (0U - 550U)
#define TCN_SELCHANGE         (TCN_FIRST - 1)
#define NM_CLICK              ((UINT)-2)
#define NM_DBLCLK             ((UINT)-3)
#define NM_RCLICK             ((UINT)-5)
#define LVN_FIRST             ((UINT)-100)
#define LVN_ITEMCHANGED       (LVN_FIRST - 1)
#define LVN_GETDISPINFOW      (LVN_FIRST - 77)
#define TCIF_TEXT             0x0001
#define TCM_INSERTITEMW       (TCM_FIRST + 62)
#define TCM_GETITEMW          (TCM_FIRST + 60)
#define TCM_GETCURSEL         (TCM_FIRST + 11)
#define TCM_SETCURSEL         (TCM_FIRST + 12)
#define TCM_SETCURFOCUS       (TCM_FIRST + 48)

#define LVM_FIRST             0x1000
#define LVM_GETITEMCOUNT      (LVM_FIRST + 4)
#define LVM_GETHEADER         (LVM_FIRST + 31)
#define LVM_GETITEMSTATE      (LVM_FIRST + 44)
#define LVM_SETITEMCOUNT      (LVM_FIRST + 47)
#define LVM_GETSELECTEDCOUNT  (LVM_FIRST + 50)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVM_DELETEALLITEMS    (LVM_FIRST + 9)
#define LVM_DELETEITEM        (LVM_FIRST + 8)
#define LVM_DELETECOLUMN      (LVM_FIRST + 28)
#define LVM_GETIMAGELIST      (LVM_FIRST + 2)
#define LVM_SETIMAGELIST      (LVM_FIRST + 3)
#define LVM_REDRAWITEMS       (LVM_FIRST + 21)
#define LVM_GETITEMW          (LVM_FIRST + 75)
#define LVM_SETITEMW          (LVM_FIRST + 76)
#define LVM_INSERTITEMW       (LVM_FIRST + 77)
#define LVM_INSERTCOLUMNW     (LVM_FIRST + 97)
#define LVM_SORTITEMS         (LVM_FIRST + 48)
#define LVM_SETCOLUMNORDERARRAY (LVM_FIRST + 58)
#define LVM_SUBITEMHITTEST    (LVM_FIRST + 57)
#define LVM_GETITEMRECT       (LVM_FIRST + 14)

#define HDM_FIRST             0x1200
#define HDM_GETITEMCOUNT      (HDM_FIRST + 0)
#define HDM_GETITEMW          (HDM_FIRST + 11)
#define HDM_GETORDERARRAY     (HDM_FIRST + 17)

#define HDN_FIRST             ((UINT)-300)
#define HDN_ENDDRAG           (HDN_FIRST - 11)
#define HDN_ITEMCLICKW        (HDN_FIRST - 22)
#define HDN_ITEMCHANGEDW      (HDN_FIRST - 21)

#define ILC_COLOR8            0x00000008
#define ILC_MASK              0x00000001

#define LPSTR_TEXTCALLBACKW   ((LPWSTR)-1)
#define LVIS_SELECTED         0x0002
#define LVIR_BOUNDS           0
#define LVIR_ICON             1
#define LVSICF_NOSCROLL       0x0002

void WINAPI InitCommonControls(void);
BOOL WINAPI InitCommonControlsEx(const INITCOMMONCONTROLSEX *lpInitCtrls);
HWND WINAPI CreateStatusWindowW(LONG style, LPCWSTR text, HWND parent, UINT id);
HIMAGELIST WINAPI ImageList_Create(int cx, int cy, UINT flags, int cInitial, int cGrow);
int WINAPI ImageList_AddIcon(HIMAGELIST himl, HICON hicon);
int WINAPI ImageList_ReplaceIcon(HIMAGELIST himl, int i, HICON hicon);
BOOL WINAPI ImageList_Remove(HIMAGELIST himl, int i);
int WINAPI ListView_InsertItemW(HWND hwnd, const LVITEMW *pitem);
int WINAPI ListView_InsertColumnW(HWND hwnd, int iCol, const LVCOLUMNW *pcol);
BOOL WINAPI ListView_SetItemTextW(HWND hwnd, int i, int iSubItem, LPWSTR pszText);
void WINAPI ListView_GetItemTextW(HWND hwnd, int i, int iSubItem, LPWSTR pszText, int cchTextMax);
void WINAPI ListView_GetItemTextA(HWND hwnd, int i, int iSubItem, LPSTR pszText, int cchTextMax);
BOOL WINAPI SetWindowSubclass(HWND hWnd, SUBCLASSPROC pfnSubclass, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT WINAPI DefSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
