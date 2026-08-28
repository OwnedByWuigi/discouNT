#ifndef DISCOUNT_OBJBASE_H
#define DISCOUNT_OBJBASE_H
#include "windows.h"
#include "guiddef.h"

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_POINTER ((HRESULT)0x80004003L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define CLASS_E_NOAGGREGATION ((HRESULT)0x80040110L)
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#define SUCCEEDED(hr) ((HRESULT)(hr)>=0)
#define FAILED(hr) ((HRESULT)(hr)<0)
#define CLSCTX_INPROC_SERVER 1
#define CLSCTX_INPROC CLSCTX_INPROC_SERVER
#define CLSCTX_LOCAL_SERVER 4
#define REGCLS_MULTIPLEUSE 1

typedef struct IUnknown IUnknown;
typedef struct IUnknownVtbl {
    HRESULT (WINAPI *QueryInterface)(IUnknown*,REFIID,void**);
    ULONG (WINAPI *AddRef)(IUnknown*);
    ULONG (WINAPI *Release)(IUnknown*);
} IUnknownVtbl;
struct IUnknown { const IUnknownVtbl *lpVtbl; };
#define IUnknown_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((IUnknown*)(p),(a),(b)))
#define IUnknown_AddRef(p) ((p)->lpVtbl->AddRef((IUnknown*)(p)))
#define IUnknown_Release(p) ((p)->lpVtbl->Release((IUnknown*)(p)))

typedef struct IClassFactory IClassFactory;
typedef struct IClassFactoryVtbl {
    HRESULT (WINAPI *QueryInterface)(IClassFactory*,REFIID,void**);
    ULONG (WINAPI *AddRef)(IClassFactory*);
    ULONG (WINAPI *Release)(IClassFactory*);
    HRESULT (WINAPI *CreateInstance)(IClassFactory*,IUnknown*,REFIID,void**);
    HRESULT (WINAPI *LockServer)(IClassFactory*,BOOL);
} IClassFactoryVtbl;
struct IClassFactory { const IClassFactoryVtbl *lpVtbl; };
#define IClassFactory_CreateInstance(p,a,b,c) ((p)->lpVtbl->CreateInstance((p),(a),(b),(c)))
#define IClassFactory_Release(p) ((p)->lpVtbl->Release((p)))
#define IClassFactory_AddRef(p) ((p)->lpVtbl->AddRef((p)))

extern const IID IID_IUnknown;
extern const IID IID_IClassFactory;
HRESULT WINAPI CoInitialize(LPVOID reserved);
HRESULT WINAPI CoInitializeEx(LPVOID reserved,DWORD flags);
void WINAPI CoUninitialize(void);
HRESULT WINAPI CoCreateInstance(REFCLSID clsid,IUnknown *outer,DWORD context,REFIID iid,void **object);
HRESULT WINAPI CLSIDFromString(LPCWSTR string, CLSID *clsid);
int WINAPI StringFromGUID2(REFGUID guid, LPWSTR string, int cchMax);
HRESULT WINAPI CoRegisterClassObject(REFCLSID clsid,IUnknown *factory,DWORD context,DWORD flags,DWORD *cookie);
HRESULT WINAPI CoRevokeClassObject(DWORD cookie);
LPVOID WINAPI CoTaskMemAlloc(SIZE_T size);
LPVOID WINAPI CoTaskMemRealloc(LPVOID memory,SIZE_T size);
void WINAPI CoTaskMemFree(LPVOID memory);
#endif
