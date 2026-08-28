#ifndef DISCOUNT_DDRAW_H
#define DISCOUNT_DDRAW_H
#include "objbase.h"
typedef struct { DWORD dwSize, dwFlags, dwFourCC, dwRGBBitCount; } DDPIXELFORMAT;
typedef struct { DWORD dwSize,dwFlags,dwHeight,dwWidth; DDPIXELFORMAT ddpfPixelFormat; } DDSURFACEDESC2;
typedef struct { DWORD dwCaps, dwCaps2, dwCaps3, dwCaps4; } DDSCAPS2;
typedef struct IDirectDraw7 IDirectDraw7;
typedef struct IDirectDraw7Vtbl { void *q[3]; HRESULT (*GetAvailableVidMem)(IDirectDraw7 *,DDSCAPS2 *,DWORD *,DWORD *); HRESULT (*GetDisplayMode)(IDirectDraw7 *,DDSURFACEDESC2 *); } IDirectDraw7Vtbl;
struct IDirectDraw7 { const IDirectDraw7Vtbl *lpVtbl; };
static const IID IID_IDirectDraw7 = {0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}};
#define DDSCAPS_VIDEOMEMORY 0x40
#define DDSCAPS_LOCALVIDMEM 0x80
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PIXELFORMAT 0x1000
HRESULT WINAPI DirectDrawCreateEx(GUID *, void **, REFIID, IUnknown *);
#define IDirectDraw7_Release(p) (((ULONG (*)(void *))(p)->lpVtbl->q[2])(p))
#define IDirectDraw7_GetAvailableVidMem(p,a,b,c) ((p)->lpVtbl->GetAvailableVidMem(p,a,b,c))
#define IDirectDraw7_GetDisplayMode(p,a) ((p)->lpVtbl->GetDisplayMode(p,a))
#endif
