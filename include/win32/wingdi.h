#ifndef DISCOUNT_WINGDI_H
#define DISCOUNT_WINGDI_H

#include "windef.h"

typedef DWORD COLORREF;

typedef struct tagLOGFONTW {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    WCHAR lfFaceName[32];
} LOGFONTW, *PLOGFONTW, *LPLOGFONTW;

typedef struct _ICONINFO {
    BOOL fIcon;
    DWORD xHotspot;
    DWORD yHotspot;
    HBITMAP hbmMask;
    HBITMAP hbmColor;
} ICONINFO, *PICONINFO;

#define TRANSPARENT 1
#define OPAQUE      2
#define ETO_OPAQUE  0x0002
#define PS_SOLID    0
#define SRCCOPY     0x00CC0020
#define SRCPAINT    0x00EE0086

#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))

BOOL WINAPI Rectangle(HDC hdc, int left, int top, int right, int bottom);
BOOL WINAPI TextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int c);
BOOL WINAPI TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
BOOL WINAPI ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT *lprect, LPCWSTR lpString, UINT c, const INT *lpDx);
COLORREF WINAPI SetTextColor(HDC hdc, COLORREF color);
COLORREF WINAPI SetBkColor(HDC hdc, COLORREF color);
int WINAPI SetBkMode(HDC hdc, int mode);
HGDIOBJ WINAPI GetStockObject(int i);
HPEN WINAPI CreatePen(int fnPenStyle, int nWidth, COLORREF crColor);
HBRUSH WINAPI CreateSolidBrush(COLORREF color);
HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ hgdiobj);
BOOL WINAPI DeleteObject(HGDIOBJ ho);
BOOL WINAPI DeleteDC(HDC hdc);
HDC WINAPI CreateCompatibleDC(HDC hdc);
HBITMAP WINAPI CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HBITMAP WINAPI LoadBitmapW(HINSTANCE hInstance, LPCWSTR lpBitmapName);
BOOL WINAPI MoveToEx(HDC hdc, int x, int y, LPPOINT lppt);
BOOL WINAPI LineTo(HDC hdc, int x, int y);
COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color);
int WINAPI FillRect(HDC hdc, const RECT *lprc, HBRUSH hbr);
BOOL WINAPI BitBlt(HDC hdcDest, int xDest, int yDest, int w, int h, HDC hdcSrc, int xSrc, int ySrc, DWORD rop);
int WINAPI SaveDC(HDC hdc);
BOOL WINAPI RestoreDC(HDC hdc, int nSavedDC);
int WINAPI ExcludeClipRect(HDC hdc, int left, int top, int right, int bottom);
HICON WINAPI CreateIconIndirect(PICONINFO piconinfo);

#endif
