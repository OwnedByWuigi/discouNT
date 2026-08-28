#ifndef DISCOUNT_D3D9_H
#define DISCOUNT_D3D9_H
#include "objbase.h"
#ifndef DISCOUNT_D3DFORMAT_DEFINED
#define DISCOUNT_D3DFORMAT_DEFINED
typedef int D3DFORMAT;
#endif
typedef struct { UINT Width, Height; D3DFORMAT Format; UINT RefreshRate; } D3DDISPLAYMODE;
typedef struct { GUID DeviceIdentifier; struct { DWORD LowPart, HighPart; } DriverVersion; DWORD VendorId, DeviceId, SubSysId, Revision; char Driver[512]; char Description[512]; char DeviceName[32]; } D3DADAPTER_IDENTIFIER9;
typedef struct { DWORD dummy[32]; } D3DCAPS9;
typedef struct IDirect3D9 IDirect3D9;
typedef struct IDirect3D9Vtbl { ULONG (WINAPI *QueryInterface)(IDirect3D9 *, REFIID, void **); ULONG (WINAPI *AddRef)(IDirect3D9 *); ULONG (WINAPI *Release)(IDirect3D9 *); UINT (WINAPI *GetAdapterCount)(IDirect3D9 *); HRESULT (WINAPI *GetAdapterIdentifier)(IDirect3D9 *, UINT, DWORD, D3DADAPTER_IDENTIFIER9 *); HRESULT (WINAPI *GetAdapterMode)(void); HRESULT (WINAPI *GetAdapterDisplayMode)(IDirect3D9 *, UINT, D3DDISPLAYMODE *); HRESULT (WINAPI *GetDeviceCaps)(IDirect3D9 *, UINT, int, D3DCAPS9 *); } IDirect3D9Vtbl;
struct IDirect3D9 { const IDirect3D9Vtbl *lpVtbl; };
#define D3D_SDK_VERSION 32
#define D3DDEVTYPE_HAL 1
#define D3DFMT_P8 41
#define D3DFMT_X1R5G5B5 24
#define D3DFMT_R5G6B5 23
#define D3DFMT_X8R8G8B8 22
#define IDirect3D9_Release(p) ((p)->lpVtbl->Release(p))
#define IDirect3D9_GetAdapterCount(p) ((p)->lpVtbl->GetAdapterCount(p))
#define IDirect3D9_GetAdapterIdentifier(p,a,b,c) ((p)->lpVtbl->GetAdapterIdentifier(p,a,b,c))
#define IDirect3D9_GetAdapterDisplayMode(p,a,b) ((p)->lpVtbl->GetAdapterDisplayMode(p,a,b))
#define IDirect3D9_GetDeviceCaps(p,a,b,c) ((p)->lpVtbl->GetDeviceCaps(p,a,b,c))
IDirect3D9 *WINAPI Direct3DCreate9(UINT version);
#endif
