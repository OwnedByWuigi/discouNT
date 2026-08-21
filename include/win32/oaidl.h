#ifndef DISCOUNT_OAIDL_H
#define DISCOUNT_OAIDL_H
#include "objbase.h"
typedef WORD VARTYPE;
typedef LONG DISPID;
typedef SHORT VARIANT_BOOL;
typedef WCHAR OLECHAR, *LPOLESTR;
typedef struct ITypeInfo ITypeInfo;
typedef ITypeInfo *LPTYPEINFO;
typedef struct ITypeLib ITypeLib;
typedef struct IDispatch IDispatch;
typedef struct tagVARIANT VARIANT;
typedef struct tagDISPPARAMS { VARIANT *rgvarg; DISPID *rgdispidNamedArgs; UINT cArgs,cNamedArgs; } DISPPARAMS;
typedef struct tagEXCEPINFO { WORD wCode,wReserved; BSTR bstrSource,bstrDescription,bstrHelpFile; DWORD dwHelpContext; LPVOID pvReserved; HRESULT (WINAPI *pfnDeferredFillIn)(struct tagEXCEPINFO*); HRESULT scode; } EXCEPINFO;
#define VT_EMPTY 0
#define VT_UI1 17
#define VT_ARRAY 0x2000
typedef struct tagSAFEARRAYBOUND { ULONG cElements; LONG lLbound; } SAFEARRAYBOUND;
typedef struct tagSAFEARRAY { WORD cDims,fFeatures; ULONG cbElements,cLocks; LPVOID pvData; SAFEARRAYBOUND rgsabound[1]; } SAFEARRAY;
typedef struct tagVARIANT { VARTYPE vt; WORD reserved1,reserved2,reserved3; union { LONG lVal; ULONG ulVal; BSTR bstrVal; IUnknown *punkVal; SAFEARRAY *parray; LPVOID byref; }; } VARIANT;
typedef struct IDispatchVtbl {
 HRESULT(WINAPI*QueryInterface)(IDispatch*,REFIID,void**); ULONG(WINAPI*AddRef)(IDispatch*); ULONG(WINAPI*Release)(IDispatch*);
 HRESULT(WINAPI*GetTypeInfoCount)(IDispatch*,UINT*); HRESULT(WINAPI*GetTypeInfo)(IDispatch*,UINT,LCID,ITypeInfo**);
 HRESULT(WINAPI*GetIDsOfNames)(IDispatch*,REFIID,LPOLESTR*,UINT,LCID,DISPID*);
 HRESULT(WINAPI*Invoke)(IDispatch*,DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);
} IDispatchVtbl;
struct IDispatch { const IDispatchVtbl *lpVtbl; };
#define IDispatch_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IDispatch_Release(p) ((p)->lpVtbl->Release((p)))
extern const IID IID_IDispatch;
typedef struct ITypeInfoVtbl { HRESULT(WINAPI*QueryInterface)(ITypeInfo*,REFIID,void**); ULONG(WINAPI*AddRef)(ITypeInfo*); ULONG(WINAPI*Release)(ITypeInfo*); HRESULT(WINAPI*GetTypeAttr)(); HRESULT(WINAPI*GetTypeComp)(); HRESULT(WINAPI*GetFuncDesc)(); HRESULT(WINAPI*GetVarDesc)(); HRESULT(WINAPI*GetNames)(); HRESULT(WINAPI*GetRefTypeOfImplType)(); HRESULT(WINAPI*GetImplTypeFlags)(); HRESULT(WINAPI*GetIDsOfNames)(ITypeInfo*,LPOLESTR*,UINT,DISPID*); HRESULT(WINAPI*Invoke)(ITypeInfo*,void*,DISPID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*); } ITypeInfoVtbl;
struct ITypeInfo { const ITypeInfoVtbl *lpVtbl; };
#define ITypeInfo_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define ITypeInfo_Release(p) ((p)->lpVtbl->Release((p)))
#define ITypeInfo_GetIDsOfNames(p,a,b,c) ((p)->lpVtbl->GetIDsOfNames((p),(a),(b),(c)))
#define ITypeInfo_Invoke(p,a,b,c,d,e,f,g) ((p)->lpVtbl->Invoke((p),(a),(b),(c),(d),(e),(f),(g)))
typedef struct ITypeLibVtbl { HRESULT(WINAPI*QueryInterface)(ITypeLib*,REFIID,void**); ULONG(WINAPI*AddRef)(ITypeLib*); ULONG(WINAPI*Release)(ITypeLib*); UINT(WINAPI*GetTypeInfoCount)(ITypeLib*); HRESULT(WINAPI*GetTypeInfo)(); HRESULT(WINAPI*GetTypeInfoType)(); HRESULT(WINAPI*GetTypeInfoOfGuid)(ITypeLib*,REFGUID,ITypeInfo**); } ITypeLibVtbl;
struct ITypeLib { const ITypeLibVtbl *lpVtbl; };
#define ITypeLib_Release(p) ((p)->lpVtbl->Release((p)))
#define ITypeLib_GetTypeInfoOfGuid(p,a,b) ((p)->lpVtbl->GetTypeInfoOfGuid((p),(a),(b)))
HRESULT WINAPI LoadRegTypeLib(REFGUID libid,WORD major,WORD minor,LCID locale,ITypeLib **library);
#define V_VT(v) ((v)->vt)
#define V_ARRAY(v) ((v)->parray)
SAFEARRAY *WINAPI SafeArrayCreateVector(VARTYPE type,LONG lower,ULONG count);
HRESULT WINAPI SafeArrayDestroy(SAFEARRAY *array);
void WINAPI VariantInit(VARIANT *value);
HRESULT WINAPI VariantClear(VARIANT *value);
BSTR WINAPI SysAllocString(const WCHAR *text);
void WINAPI SysFreeString(BSTR text);
#endif
