#ifndef DISCOUNT_WBEMCLI_H
#define DISCOUNT_WBEMCLI_H
#include "objbase.h"
#include "oaidl.h"
#ifndef CLASS_E_CLASSNOTAVAILABLE
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111L)
#endif
#define WBEM_FLAG_SYSTEM_ONLY 0x30
typedef struct IWbemLocator IWbemLocator;
typedef struct IWbemServices IWbemServices;
typedef struct IWbemClassObject IWbemClassObject;
typedef struct IEnumWbemClassObject IEnumWbemClassObject;
typedef struct IWbemLocatorVtbl { void *q[3]; HRESULT (*ConnectServer)(IWbemLocator*,BSTR,LPCWSTR,LPCWSTR,LPCWSTR,LONG,LPCWSTR,IUnknown*,IWbemServices**); } IWbemLocatorVtbl;
struct IWbemLocator { const IWbemLocatorVtbl *lpVtbl; };
typedef struct IWbemServicesVtbl { void *q[3]; HRESULT (*CreateInstanceEnum)(IWbemServices*,BSTR,LONG,IUnknown*,IEnumWbemClassObject**); } IWbemServicesVtbl;
struct IWbemServices { const IWbemServicesVtbl *lpVtbl; };
typedef struct IWbemClassObjectVtbl { void *q[3]; HRESULT (*Get)(IWbemClassObject*,LPCWSTR,LONG,VARIANT*,LONG*,LONG*); } IWbemClassObjectVtbl;
struct IWbemClassObject { const IWbemClassObjectVtbl *lpVtbl; };
typedef struct IEnumWbemClassObjectVtbl { void *q[3]; HRESULT (*Next)(IEnumWbemClassObject*,LONG,ULONG,IWbemClassObject**,ULONG*); } IEnumWbemClassObjectVtbl;
struct IEnumWbemClassObject { const IEnumWbemClassObjectVtbl *lpVtbl; };
#define IWbemLocator_ConnectServer(p,a,b,c,d,e,f,g,h) ((p)->lpVtbl->ConnectServer(p,a,b,c,d,e,f,g,h))
#define IWbemLocator_Release(p) (((ULONG (*)(void *))(p)->lpVtbl->q[2])(p))
#define IWbemServices_CreateInstanceEnum(p,a,b,c,d) ((p)->lpVtbl->CreateInstanceEnum(p,a,b,c,d))
#define IWbemServices_Release(p) (((ULONG (*)(void *))(p)->lpVtbl->q[2])(p))
#define IWbemClassObject_Get(p,a,b,c,d,e) ((p)->lpVtbl->Get(p,a,b,c,d,e))
#define IWbemClassObject_Release(p) (((ULONG (*)(void *))(p)->lpVtbl->q[2])(p))
#define IEnumWbemClassObject_Next(p,a,b,c,d) ((p)->lpVtbl->Next(p,a,b,c,d))
#define IEnumWbemClassObject_Release(p) (((ULONG (*)(void *))(p)->lpVtbl->q[2])(p))
static const CLSID CLSID_WbemLocator = {0x4590f811,0x1d3a,0x11d0,{0x89,0x1f,0x00,0xaa,0x00,0x4b,0x2e,0x24}};
static const IID IID_IWbemLocator = {0xdc12a687,0x737f,0x11cf,{0x88,0x4d,0x00,0xaa,0x00,0x4b,0x2e,0x24}};
static const IID IID_IWbemServices = {0x9556dc99,0x828c,0x11cf,{0xa3,0x7e,0x00,0xaa,0x00,0x32,0x40,0xc7}};
static const IID IID_IWbemClassObject = {0xdc12a681,0x737f,0x11cf,{0x88,0x4d,0x00,0xaa,0x00,0x4b,0x2e,0x24}};
#endif
