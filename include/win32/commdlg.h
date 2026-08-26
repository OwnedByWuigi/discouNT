#ifndef DISCOUNT_COMMDLG_H
#define DISCOUNT_COMMDLG_H

#include "windows.h"

typedef UINT_PTR (CALLBACK *LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagOFNW {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HINSTANCE    hInstance;
    LPCWSTR      lpstrFilter;
    LPWSTR       lpstrCustomFilter;
    DWORD        nMaxCustFilter;
    DWORD        nFilterIndex;
    LPWSTR       lpstrFile;
    DWORD        nMaxFile;
    LPWSTR       lpstrFileTitle;
    DWORD        nMaxFileTitle;
    LPCWSTR      lpstrInitialDir;
    LPCWSTR      lpstrTitle;
    DWORD        Flags;
    WORD         nFileOffset;
    WORD         nFileExtension;
    LPCWSTR      lpstrDefExt;
    LPARAM       lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCWSTR      lpTemplateName;
    void        *pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;
} OPENFILENAMEW, *LPOPENFILENAMEW;

typedef struct tagOFNA {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HINSTANCE    hInstance;
    LPCSTR       lpstrFilter;
    LPSTR        lpstrCustomFilter;
    DWORD        nMaxCustFilter;
    DWORD        nFilterIndex;
    LPSTR        lpstrFile;
    DWORD        nMaxFile;
    LPSTR        lpstrFileTitle;
    DWORD        nMaxFileTitle;
    LPCSTR       lpstrInitialDir;
    LPCSTR       lpstrTitle;
    DWORD        Flags;
    WORD         nFileOffset;
    WORD         nFileExtension;
    LPCSTR       lpstrDefExt;
    LPARAM       lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCSTR       lpTemplateName;
    void        *pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;
} OPENFILENAMEA, *LPOPENFILENAMEA;

typedef struct tagCHOOSEFONTW {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HDC          hDC;
    LPLOGFONTW   lpLogFont;
    INT          iPointSize;
    DWORD        Flags;
    COLORREF     rgbColors;
    LPARAM       lCustData;
    void        *lpfnHook;
    LPCWSTR      lpTemplateName;
    HINSTANCE    hInstance;
    LPWSTR       lpszStyle;
    WORD         nFontType;
    WORD         ___MISSING_ALIGNMENT__;
    INT          nSizeMin;
    INT          nSizeMax;
} CHOOSEFONTW, *LPCHOOSEFONTW;

typedef struct tagFRW {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HINSTANCE    hInstance;
    DWORD        Flags;
    LPWSTR       lpstrFindWhat;
    LPWSTR       lpstrReplaceWith;
    WORD         wFindWhatLen;
    WORD         wReplaceWithLen;
    LPARAM       lCustData;
    void        *lpfnHook;
    LPCWSTR      lpTemplateName;
} FINDREPLACEW, *LPFINDREPLACEW;

typedef struct tagOFNOTIFYW {
    NMHDR hdr;
    LPOPENFILENAMEW lpOFN;
    LPWSTR pszFile;
} OFNOTIFYW, *LPOFNOTIFYW;

typedef struct tagPDW {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HGLOBAL      hDevMode;
    HGLOBAL      hDevNames;
    HDC          hDC;
    DWORD        Flags;
    WORD         nFromPage;
    WORD         nToPage;
    WORD         nMinPage;
    WORD         nMaxPage;
    WORD         nCopies;
    HINSTANCE    hInstance;
    LPARAM       lCustData;
    void        *lpfnPrintHook;
    void        *lpfnSetupHook;
    LPCWSTR      lpPrintTemplateName;
    LPCWSTR      lpSetupTemplateName;
    HGLOBAL      hPrintTemplate;
    HGLOBAL      hSetupTemplate;
} PRINTDLGW, *LPPRINTDLGW;

#define OFN_READONLY            0x00000001
#define OFN_OVERWRITEPROMPT     0x00000002
#define OFN_HIDEREADONLY        0x00000004
#define OFN_ENABLEHOOK          0x00000020
#define OFN_ENABLETEMPLATE      0x00000040
#define OFN_EXPLORER            0x00080000
#define OFN_ENABLESIZING        0x00800000
#define OFN_PATHMUSTEXIST       0x00000800
#define OFN_FILEMUSTEXIST       0x00001000

#define CDN_FIRST               ((UINT)-601)
#define CDN_SELCHANGE           (CDN_FIRST - 1)
#define CDM_FIRST               (WM_USER + 100)
#define CDM_GETFILEPATH         (CDM_FIRST + 1)

#define FINDMSGSTRINGW          L"commdlg_FindReplace"

#define FR_DOWN                 0x00000001
#define FR_WHOLEWORD            0x00000002
#define FR_MATCHCASE            0x00000004
#define FR_FINDNEXT             0x00000008
#define FR_REPLACE              0x00000010
#define FR_REPLACEALL           0x00000020
#define FR_DIALOGTERM           0x00000040
#define FR_SHOWHELP             0x00000080
#define FR_ENABLEHOOK           0x00000100
#define FR_HIDEWHOLEWORD        0x00010000

#define CF_SCREENFONTS          0x00000001
#define CF_INITTOLOGFONTSTRUCT  0x00000040
#define CF_NOSCRIPTSEL          0x00800000
#define CF_NOVERTFONTS          0x01000000

#define PD_PAGENUMS             0x00000002
#define PD_RETURNDC             0x00000100
#define PD_NOPAGENUMS           0x00000008
#define PD_NOSELECTION          0x00000004
#define PD_PRINTSETUP           0x00000040
#define PD_PRINTTOFILE          0x00000020
#define PD_USEDEVMODECOPIES     0x00040000

BOOL WINAPI GetOpenFileNameW(LPOPENFILENAMEW ofn);
BOOL WINAPI GetOpenFileNameA(LPOPENFILENAMEA ofn);
BOOL WINAPI GetSaveFileNameW(LPOPENFILENAMEW ofn);
BOOL WINAPI ChooseFontW(LPCHOOSEFONTW cf);
HWND WINAPI FindTextW(LPFINDREPLACEW fr);
HWND WINAPI ReplaceTextW(LPFINDREPLACEW fr);
BOOL WINAPI PrintDlgW(LPPRINTDLGW pd);
DWORD WINAPI CommDlgExtendedError(void);

#endif
