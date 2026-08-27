#ifndef DISCOUNT_WINGDI_H
#define DISCOUNT_WINGDI_H

#include "windef.h"

typedef DWORD COLORREF;
typedef struct tagBITMAP { LONG bmType; LONG bmWidth; LONG bmHeight; LONG bmWidthBytes; WORD bmPlanes; WORD bmBitsPixel; LPVOID bmBits; } BITMAP, *PBITMAP;
typedef struct tagRGBQUAD { BYTE rgbBlue,rgbGreen,rgbRed,rgbReserved; } RGBQUAD;
typedef struct tagBITMAPINFOHEADER { DWORD biSize; LONG biWidth,biHeight; WORD biPlanes,biBitCount; DWORD biCompression,biSizeImage; LONG biXPelsPerMeter,biYPelsPerMeter; DWORD biClrUsed,biClrImportant; } BITMAPINFOHEADER;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO;
typedef struct _BLENDFUNCTION { BYTE BlendOp,BlendFlags,SourceConstantAlpha,AlphaFormat; } BLENDFUNCTION;
int WINAPI GetObject(HGDIOBJ object, int bytes, LPVOID buffer);
int WINAPI GetObjectW(HGDIOBJ object, int bytes, LPVOID buffer);
HBITMAP WINAPI CreateBitmap(int width, int height, UINT planes, UINT bits, const void *bits_data);
HBITMAP WINAPI CreateDIBitmap(HDC hdc, const BITMAPINFOHEADER *header, DWORD init,
                              const void *bits, const BITMAPINFO *info, UINT usage);
HDC WINAPI CreateEnhMetaFileW(HDC hdc, LPCWSTR filename, const RECT *rect, LPCWSTR description);
HENHMETAFILE WINAPI CloseEnhMetaFile(HDC hdc);
UINT WINAPI GetEnhMetaFileBits(HENHMETAFILE emf, UINT size, BYTE *bits);
BOOL WINAPI DeleteEnhMetaFile(HENHMETAFILE emf);

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

typedef struct tagLOGFONTA {
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
    char lfFaceName[32];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

typedef struct _ICONINFO {
    BOOL fIcon;
    DWORD xHotspot;
    DWORD yHotspot;
    HBITMAP hbmMask;
    HBITMAP hbmColor;
} ICONINFO, *PICONINFO;

typedef struct tagDOCINFOW {
    INT cbSize;
    LPCWSTR lpszDocName;
    LPCWSTR lpszOutput;
    LPCWSTR lpszDatatype;
    DWORD fwType;
} DOCINFOW, *LPDOCINFOW;

#define TRANSPARENT 1
#define OPAQUE      2
#define ETO_OPAQUE  0x0002
#define ETO_CLIPPED 0x0004
#define PS_SOLID    0
#define SRCCOPY     0x00CC0020
#define SRCPAINT    0x00EE0086
#define SRCAND      0x008800C6
#define CBM_INIT    0x00000004
#define BI_RGB 0
#define DIB_RGB_COLORS 0
#define AC_SRC_OVER 0
#define AC_SRC_ALPHA 1

#define FW_REGULAR          400
#define ANSI_CHARSET        0
#define DEFAULT_CHARSET     1
#define SHIFTJIS_CHARSET    128
#define HANGEUL_CHARSET     129
#define GB2312_CHARSET      134
#define CHINESEBIG5_CHARSET 136
#define GREEK_CHARSET       161
#define TURKISH_CHARSET     162
#define HEBREW_CHARSET      177
#define ARABIC_CHARSET      178
#define BALTIC_CHARSET      186
#define VIETNAMESE_CHARSET  163
#define RUSSIAN_CHARSET     204
#define EE_CHARSET          238
#define THAI_CHARSET        222
#define JOHAB_CHARSET       130
#define MAC_CHARSET         77
#define OUT_DEFAULT_PRECIS  0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY     0
#define FIXED_PITCH         1
#define DEFAULT_PITCH       0
#define FF_DONTCARE         0
#define FF_MODERN           0x30
#define FF_ROMAN            0x10
#define FF_SWISS            0x20
#define FF_SCRIPT           0x40
#define FF_DECORATIVE       0x50
#define LF_FACESIZE         32
#define GetRValue(rgb) ((BYTE)((rgb) & 0xff))
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xff))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xff))

#define LOGPIXELSX          88
#define LOGPIXELSY          90
#define PHYSICALWIDTH       110
#define PHYSICALHEIGHT      111
#define PHYSICALOFFSETX     112
#define PHYSICALOFFSETY     113
#define MM_TEXT             1

typedef struct tagTEXTMETRICW {
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    WCHAR tmFirstChar;
    WCHAR tmLastChar;
    WCHAR tmDefaultChar;
    WCHAR tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
} TEXTMETRICW, *LPTEXTMETRICW;

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
HBITMAP WINAPI CreateDIBSection(HDC dc,const BITMAPINFO *info,UINT usage,void **bits,HANDLE section,DWORD offset);
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
HFONT WINAPI CreateFontIndirectW(const LOGFONTW *lplf);
int WINAPI GetDeviceCaps(HDC hdc, int index);
BOOL WINAPI GetTextMetricsW(HDC hdc, LPTEXTMETRICW lptm);
BOOL WINAPI GetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int c, LPSIZE psizl);
BOOL WINAPI GetTextExtentPointA(HDC dc,LPCSTR text,int count,LPSIZE size);
BOOL WINAPI GetTextExtentPointW(HDC dc,LPCWSTR text,int count,LPSIZE size);
BOOL WINAPI GetTextExtentExPointW(HDC hdc, LPCWSTR lpszStr, int cchString, int nMaxExtent,
                                  LPINT lpnFit, LPINT alpDx, LPSIZE lpSize);
int WINAPI SetMapMode(HDC hdc, int mode);
int WINAPI StartDocW(HDC hdc, const DOCINFOW *lpdi);
int WINAPI StartPage(HDC hdc);
int WINAPI EndPage(HDC hdc);
int WINAPI EndDoc(HDC hdc);

#endif
