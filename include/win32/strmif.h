#ifndef DISCOUNT_STRMIF_H
#define DISCOUNT_STRMIF_H
#include "objbase.h"
typedef struct IMoniker IMoniker; typedef struct IEnumMoniker IEnumMoniker; typedef struct ICreateDevEnum ICreateDevEnum; typedef struct IPropertyBag IPropertyBag; typedef struct IFilterMapper2 IFilterMapper2; typedef struct IAMFilterData IAMFilterData;
typedef struct IMonikerVtbl {
    HRESULT (*QueryInterface)(IMoniker*,REFIID,void**); ULONG (*AddRef)(IMoniker*); ULONG (*Release)(IMoniker*);
    HRESULT (*GetClassID)(IMoniker*,CLSID*); HRESULT (*IsDirty)(IMoniker*); HRESULT (*Load)(IMoniker*,IUnknown*);
    HRESULT (*Save)(IMoniker*,IUnknown*,BOOL); HRESULT (*GetSizeMax)(IMoniker*,ULARGE_INTEGER*);
    HRESULT (*BindToObject)(IMoniker*,IUnknown*,IUnknown*,REFIID,void**);
    HRESULT (*BindToStorage)(IMoniker*,IUnknown*,IUnknown*,REFIID,void**);
} IMonikerVtbl;
struct IMoniker { const IMonikerVtbl *lpVtbl; };
typedef struct IEnumMonikerVtbl { HRESULT (*QueryInterface)(IEnumMoniker*,REFIID,void**); ULONG (*AddRef)(IEnumMoniker*); ULONG (*Release)(IEnumMoniker*); HRESULT (*Next)(IEnumMoniker*,ULONG,IMoniker**,ULONG*); } IEnumMonikerVtbl;
struct IEnumMoniker { const IEnumMonikerVtbl *lpVtbl; };
typedef struct ICreateDevEnumVtbl { HRESULT (*QueryInterface)(ICreateDevEnum*,REFIID,void**); ULONG (*AddRef)(ICreateDevEnum*); ULONG (*Release)(ICreateDevEnum*); HRESULT (*CreateClassEnumerator)(ICreateDevEnum*,REFCLSID,IEnumMoniker**,DWORD); } ICreateDevEnumVtbl;
struct ICreateDevEnum { const ICreateDevEnumVtbl *lpVtbl; };
typedef struct IPropertyBagVtbl { HRESULT (*QueryInterface)(IPropertyBag*,REFIID,void**); ULONG (*AddRef)(IPropertyBag*); ULONG (*Release)(IPropertyBag*); HRESULT (*Read)(IPropertyBag*,LPCWSTR,VARIANT*,IUnknown*); HRESULT (*Write)(IPropertyBag*,LPCWSTR,VARIANT*); } IPropertyBagVtbl;
struct IPropertyBag { const IPropertyBagVtbl *lpVtbl; };
typedef struct IFilterMapper2Vtbl { HRESULT (*QueryInterface)(IFilterMapper2*,REFIID,void**); ULONG (*AddRef)(IFilterMapper2*); ULONG (*Release)(IFilterMapper2*); } IFilterMapper2Vtbl;
struct IFilterMapper2 { const IFilterMapper2Vtbl *lpVtbl; };
typedef struct IAMFilterDataVtbl { HRESULT (*QueryInterface)(IAMFilterData*,REFIID,void**); ULONG (*AddRef)(IAMFilterData*); ULONG (*Release)(IAMFilterData*); HRESULT (*ParseFilterData)(IAMFilterData*,BYTE*,ULONG,BYTE**); } IAMFilterDataVtbl;
struct IAMFilterData { const IAMFilterDataVtbl *lpVtbl; };
typedef struct { BOOL bOutput; } REGFILTERPIN;
typedef struct { DWORD dwFlags; } REGFILTERPIN2;
typedef struct { DWORD dwVersion; DWORD cPins; REGFILTERPIN *rgPins; DWORD cPins2; REGFILTERPIN2 *rgPins2; DWORD dwMerit; } REGFILTER2;
#define REG_PINFLAG_B_OUTPUT 1
static const CLSID CLSID_SystemDeviceEnum={0x62a1,0x1,0x11ce,{0x92,0x41,0x00,0x20,0xaf,0x0,0x41,0x59}};
static const CLSID CLSID_FilterMapper2={0xcda42200,0xbd88,0x11d0,{0xbd,0x4e,0x00,0xa0,0xc9,0x11,0xce,0x86}};
static const CLSID CLSID_ActiveMovieCategories={0xda4e3da0,0xd07d,0x11d0,{0xbe,0x50,0x00,0xa0,0xc9,0x11,0xce,0x86}};
static const IID IID_ICreateDevEnum={0x29840822,0x5b84,0x11d0,{0xbd,0x3b,0x00,0xa0,0xc9,0x11,0xce,0x86}};
static const IID IID_IPropertyBag={0x55272a00,0x42cb,0x11ce,{0x81,0x35,0x00,0xaa,0x00,0x4b,0xb8,0x51}};
static const IID IID_IFilterMapper2={0xb79bb0b0,0x33c1,0x11d1,{0xab,0xe1,0x00,0xa0,0xc9,0x05,0xf3,0x75}};
static const IID IID_IAMFilterData={0x97f7c4d7,0x547b,0x4a5f,{0x83,0x3b,0x2e,0x1e,0x82,0x3a,0x5d,0x5b}};
#define IMoniker_BindToStorage(p,a,b,c,d) ((p)->lpVtbl->BindToStorage(p,a,b,c,d))
#define IMoniker_Release(p) ((p)->lpVtbl->Release(p))
#define IEnumMoniker_Next(p,a,b,c) ((p)->lpVtbl->Next(p,a,b,c))
#define IEnumMoniker_Release(p) ((p)->lpVtbl->Release(p))
#define ICreateDevEnum_CreateClassEnumerator(p,a,b,c) ((p)->lpVtbl->CreateClassEnumerator(p,a,b,c))
#define ICreateDevEnum_Release(p) ((p)->lpVtbl->Release(p))
#define IPropertyBag_Read(p,a,b,c) ((p)->lpVtbl->Read(p,a,b,c))
#define IPropertyBag_Release(p) ((p)->lpVtbl->Release(p))
#define IFilterMapper2_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface(p,a,b))
#define IFilterMapper2_Release(p) ((p)->lpVtbl->Release(p))
#define IAMFilterData_ParseFilterData(p,a,b,c) ((p)->lpVtbl->ParseFilterData(p,a,b,c))
#define IAMFilterData_Release(p) ((p)->lpVtbl->Release(p))
#endif
