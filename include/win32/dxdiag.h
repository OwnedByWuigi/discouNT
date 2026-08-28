#ifndef DISCOUNT_DXDIAG_H
#define DISCOUNT_DXDIAG_H

#include "objbase.h"
#include "oaidl.h"

#ifndef CLASS_E_CLASSNOTAVAILABLE
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111L)
#endif

#define DXDIAG_DX9_SDK_VERSION 0x0900

typedef struct _DXDIAG_INIT_PARAMS {
    DWORD dwSize;
    DWORD dwDxDiagHeaderVersion;
    BOOL bAllowWHQLChecks;
    BOOL bWHQLChecksForced;
} DXDIAG_INIT_PARAMS;

typedef struct IDxDiagProvider IDxDiagProvider;
typedef struct IDxDiagContainer IDxDiagContainer;
typedef IClassFactory *LPCLASSFACTORY;

typedef struct IDxDiagProviderVtbl {
    HRESULT (WINAPI *QueryInterface)(IDxDiagProvider *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IDxDiagProvider *);
    ULONG (WINAPI *Release)(IDxDiagProvider *);
    HRESULT (WINAPI *Initialize)(IDxDiagProvider *, DXDIAG_INIT_PARAMS *);
    HRESULT (WINAPI *GetRootContainer)(IDxDiagProvider *, IDxDiagContainer **);
} IDxDiagProviderVtbl;
struct IDxDiagProvider { const IDxDiagProviderVtbl *lpVtbl; };

typedef struct IDxDiagContainerVtbl {
    HRESULT (WINAPI *QueryInterface)(IDxDiagContainer *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IDxDiagContainer *);
    ULONG (WINAPI *Release)(IDxDiagContainer *);
    HRESULT (WINAPI *GetNumberOfChildContainers)(IDxDiagContainer *, DWORD *);
    HRESULT (WINAPI *EnumChildContainerNames)(IDxDiagContainer *, DWORD, WCHAR *, DWORD);
    HRESULT (WINAPI *GetChildContainer)(IDxDiagContainer *, LPCWSTR, IDxDiagContainer **);
    HRESULT (WINAPI *GetNumberOfProps)(IDxDiagContainer *, DWORD *);
    HRESULT (WINAPI *EnumPropNames)(IDxDiagContainer *, DWORD, WCHAR *, DWORD);
    HRESULT (WINAPI *GetProp)(IDxDiagContainer *, LPCWSTR, VARIANT *);
} IDxDiagContainerVtbl;
struct IDxDiagContainer { const IDxDiagContainerVtbl *lpVtbl; };

#define IDxDiagProvider_Initialize(p,a) ((p)->lpVtbl->Initialize((p),(a)))
#define IDxDiagProvider_GetRootContainer(p,a) ((p)->lpVtbl->GetRootContainer((p),(a)))
#define IDxDiagProvider_Release(p) ((p)->lpVtbl->Release((p)))
#define IDxDiagProvider_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IDxDiagContainer_GetChildContainer(p,a,b) ((p)->lpVtbl->GetChildContainer((p),(a),(b)))
#define IDxDiagContainer_GetProp(p,a,b) ((p)->lpVtbl->GetProp((p),(a),(b)))
#define IDxDiagContainer_Release(p) ((p)->lpVtbl->Release((p)))
#define IDxDiagContainer_AddRef(p) ((p)->lpVtbl->AddRef((p)))

#if defined(INITGUID) || defined(DXDIAG_DEFINE_GUIDS)
const CLSID CLSID_DxDiagProvider =
    {0xA65B8071, 0x3BFE, 0x4213, {0x9A, 0x5B, 0x49, 0x1D, 0xA4, 0x46, 0x1C, 0xA7}};
const IID IID_IDxDiagProvider =
    {0x0D6F9F2E, 0x1B7B, 0x42A0, {0xB5, 0x9C, 0xE4, 0x3A, 0xB5, 0x1C, 0xA0, 0x8B}};
const IID IID_IDxDiagContainer =
    {0x0D6F9F2F, 0x1B7B, 0x42A0, {0xB5, 0x9C, 0xE4, 0x3A, 0xB5, 0x1C, 0xA0, 0x8B}};
#else
extern const CLSID CLSID_DxDiagProvider;
extern const IID IID_IDxDiagProvider;
extern const IID IID_IDxDiagContainer;
#endif

#endif
