#ifndef DISCOUNT_COMMCTRL_H
#define DISCOUNT_COMMCTRL_H

#include "winuser.h"

typedef HANDLE HPROPSHEETPAGE;
typedef struct _PROPSHEETPAGEA {
    DWORD dwSize, dwFlags;
    HINSTANCE hInstance;
    LPCSTR pszTemplate;
    HANDLE hIcon;
    LPCSTR pszTitle;
    DLGPROC pfnDlgProc;
    LPARAM lParam;
} PROPSHEETPAGEA;
typedef struct _PROPSHEETHEADERA {
    DWORD dwSize, dwFlags;
    HWND hwndParent;
    HINSTANCE hInstance;
    HANDLE hIcon;
    LPCSTR pszCaption;
    UINT nPages, nStartPage;
    HPROPSHEETPAGE *phpage;
} PROPSHEETHEADERA;
#define PSH_NOAPPLYNOW     0x00000080
#define PSBTN_OK            0
#define PSM_PRESSBUTTON    (WM_USER + 113)
#define PSN_APPLY          ((UINT)-202)
#define PSNRET_NOERROR     0
#define PSNRET_INVALID     1
HPROPSHEETPAGE WINAPI CreatePropertySheetPageA(const PROPSHEETPAGEA *page);
int WINAPI PropertySheetA(const PROPSHEETHEADERA *header);

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

typedef struct tagCOMBOBOXEXITEMW { UINT mask; INT_PTR iItem; LPWSTR pszText; int cchTextMax; int iImage; int iSelectedImage; int iOverlay; int iIndent; LPARAM lParam; } COMBOBOXEXITEMW;
typedef struct tagNMCOMBOBOXEXW { NMHDR hdr; COMBOBOXEXITEMW ceItem; } NMCOMBOBOXEXW;
#define CBEMAXSTRLEN 260
typedef struct tagNMCBEENDEDITW { NMHDR hdr; BOOL fChanged; int iNewSelection; WCHAR szText[CBEMAXSTRLEN]; int iWhy; } NMCBEENDEDITW;
typedef struct tagNMCBEENDEDITA { NMHDR hdr; BOOL fChanged; int iNewSelection; char szText[CBEMAXSTRLEN]; int iWhy; } NMCBEENDEDITA;
typedef struct tagTBADDBITMAP { HINSTANCE hInst; UINT_PTR nID; } TBADDBITMAP;
typedef struct tagTBBUTTON { int iBitmap; int idCommand; BYTE fsState; BYTE fsStyle; BYTE bReserved[2]; DWORD_PTR dwData; INT_PTR iString; } TBBUTTON;
typedef struct tagREBARBANDINFOW { UINT cbSize,fMask,fStyle; COLORREF clrFore,clrBack; LPWSTR lpText; UINT cch; int iImage; HWND hwndChild; UINT cxMinChild,cyMinChild,cx; HBITMAP hbmBack; UINT wID,cyChild,cyMaxChild,cyIntegral,cxIdeal; LPARAM lParam; UINT cxHeader; } REBARBANDINFOW;
typedef struct tagNMRBAUTOSIZE { NMHDR hdr; BOOL fChanged; RECT rcTarget; RECT rcActual; } NMRBAUTOSIZE;
typedef struct tagTOOLINFOW { UINT cbSize,uFlags; HWND hwnd; UINT_PTR uId; RECT rect; HINSTANCE hinst; LPWSTR lpszText; LPARAM lParam; void *lpReserved; } TTTOOLINFOW;

#define ICC_LISTVIEW_CLASSES  0x00000001
#define ICC_BAR_CLASSES       0x00000004
#define ICC_TAB_CLASSES       0x00000008
#define ICC_COOL_CLASSES      0x00000400
#define ICC_USEREX_CLASSES    0x00000200
#define ICC_STANDARD_CLASSES 0x00004000
#define CBEIF_TEXT 0x00000001
#define CBEIF_IMAGE 0x00000002
#define CBEIF_SELECTEDIMAGE 0x00000004
#define CBEIF_OVERLAY 0x00000008
#define CBEIF_INDENT 0x00000010
#define CBEIF_LPARAM 0x00000020
#define CBEM_INSERTITEMW (WM_USER + 11)
#define CBEM_SETIMAGELIST (WM_USER + 2)
#define CBEM_GETEDITCONTROL (WM_USER + 7)
#define CBEM_SETITEMW (WM_USER + 12)
#define CBEN_FIRST ((UINT)-800)
#define CBEN_DELETEITEM (CBEN_FIRST - 1)
#define CBEN_BEGINEDIT (CBEN_FIRST - 4)
#define CBEN_ENDEDITA (CBEN_FIRST - 5)
#define CBEN_ENDEDITW (CBEN_FIRST - 6)
#define WC_COMBOBOXEXW L"ComboBoxEx32"
#define REBARCLASSNAMEW L"ReBarWindow32"
#define TOOLBARCLASSNAMEW L"ToolbarWindow32"
#define RBN_FIRST ((UINT)-831)
#define RBN_AUTOSIZE (RBN_FIRST - 3)
#define RB_INSERTBANDW (WM_USER + 10)
#define RB_SETBARINFO (WM_USER + 4)
#define RBBS_BREAK 0x00000001
#define RBBS_CHILDEDGE 0x00000004
#define RBBS_FIXEDBMP 0x00000020
#define RBBIM_STYLE 0x00000001
#define RBBIM_TEXT 0x00000004
#define RBBIM_CHILD 0x00000010
#define RBBIM_CHILDSIZE 0x00000020
#define RBBIM_SIZE 0x00000040
#define TB_BUTTONSTRUCTSIZE (WM_USER + 30)
#define TB_ADDBITMAP (WM_USER + 19)
#define TB_ADDBUTTONSW (WM_USER + 68)
#define TBSTATE_ENABLED 0x04
#define TBSTYLE_BUTTON 0x00
#define TBSTYLE_FLAT 0x0800
#define CCS_NORESIZE 0x00000004
#define CCS_NOPARENTALIGN 0x00000008
#define CCS_TOP 0x00000001
#define CCS_NODIVIDER 0x00000040
#define RBS_VARHEIGHT 0x00000200
#define RBBS_GRIPPERALWAYS 0x00000080
#define TBSTYLE_EX_MIXEDBUTTONS 0x00000008
#define BTNS_BUTTON 0x00
#define BTNS_AUTOSIZE 0x10
#define HINST_COMMCTRL ((HINSTANCE)(LONG_PTR)-1)
#define IDB_HIST_LARGE_COLOR 9
#define IDB_VIEW_LARGE_COLOR 6
#define HIST_BACK 0
#define HIST_FORWARD 1
#define VIEW_PARENTFOLDER 8
#define CBENF_DROPDOWN 4
#define CBENF_RETURN 2
#define CBENF_ESCAPE 3
#define TOOLTIPS_CLASSW L"tooltips_class32"
#define TTS_ALWAYSTIP 1
#define TTS_NOPREFIX 2
#define TTS_BALLOON 0x40
#define TTS_CLOSE 0x80
#define TTF_IDISHWND 1
#define TTF_SUBCLASS 0x10
#define TTF_TRACK 0x20
#define TTM_ADDTOOLW (WM_USER+50)
#define TTM_NEWTOOLRECTW (WM_USER+52)
#define TTM_UPDATETIPTEXTW (WM_USER+57)
#define TTM_TRACKACTIVATE (WM_USER+17)
#define TTM_TRACKPOSITION (WM_USER+18)
#define TTM_SETTITLEW (WM_USER+33)
#define TTM_RELAYEVENT (WM_USER+7)
#define TTI_ERROR 3
#define ILD_NORMAL 0
BOOL WINAPI ImageList_Draw(HIMAGELIST image_list,int image,HDC dc,int x,int y,UINT style);

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
