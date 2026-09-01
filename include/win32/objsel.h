#ifndef DISCOUNT_OBJSEL_H
#define DISCOUNT_OBJSEL_H
#include <windows.h>
typedef struct IDataObject IDataObject;
typedef struct IDsObjectPicker IDsObjectPicker;
typedef unsigned short CLIPFORMAT;
typedef struct { void *ptd; DWORD dwAspect; LONG lindex; DWORD tymed; CLIPFORMAT cfFormat; } FORMATETC;
typedef struct { DWORD tymed; union { HGLOBAL hGlobal; HANDLE hHandle; }; } STGMEDIUM;
struct IDataObject { struct { HRESULT (*QueryInterface)(IDataObject*, REFIID, void**); ULONG (*AddRef)(IDataObject*); ULONG (*Release)(IDataObject*); HRESULT (*GetData)(IDataObject*, FORMATETC*, STGMEDIUM*); } *lpVtbl; };
#define DVASPECT_CONTENT 1
#define TYMED_HGLOBAL 1
#define CFSTR_DSOP_DS_SELECTION_LIST L"DsSelectionList"
typedef struct { ULONG cbSize; LPCWSTR pwzTargetComputer; ULONG cDsScopeInfos; void *aDsScopeInfos; ULONG flOptions; ULONG cAttributesToFetch; LPCWSTR *apwzAttributeNames; } DSOP_INIT_INFO;
typedef struct { ULONG cbSize; ULONG flType; ULONG flScope; struct { struct { ULONG grfScope; ULONG flType; ULONG flScope; } up; ULONG down; } Filter; LPCWSTR pwzTargetComputer; void *pclsid; HRESULT hr; } DSOP_SCOPE_INIT_INFO;
typedef struct { HRESULT (*QueryInterface)(IDsObjectPicker*, REFIID, void**); ULONG (*AddRef)(IDsObjectPicker*); ULONG (*Release)(IDsObjectPicker*); HRESULT (*Initialize)(IDsObjectPicker*, DSOP_INIT_INFO*); HRESULT (*InvokeDialog)(IDsObjectPicker*, HWND, IDataObject**); } IDsObjectPickerVtbl;
struct IDsObjectPicker { const IDsObjectPickerVtbl *lpVtbl; };
typedef struct { WCHAR *pwzName; WCHAR *pwzClass; WCHAR *pwzUPN; WCHAR *pwzDN; WCHAR *pwzADsPath; WCHAR *pwzObjectSid; } DS_SELECTION;
typedef struct { ULONG cItems; DS_SELECTION aDsSelection[1]; } DS_SELECTION_LIST, *PDS_SELECTION_LIST;
typedef struct { ULONG cbSize; ULONG flType; ULONG flScope; struct { ULONG flType; ULONG flScope; } Filter; LPCWSTR pwzTargetComputer; void *pclsid; HRESULT hr; } DSOP_SCOPE_INIT_INFO_COMPAT;
#define DSOP_SCOPE_TYPE_USER_ENTERED_UPLEVEL_SCOPE 0x00000001
#define DSOP_SCOPE_TYPE_USER_ENTERED_DOWNLEVEL_SCOPE 0x00000002
#define DSOP_SCOPE_TYPE_GLOBAL_CATALOG 0x00000004
#define DSOP_SCOPE_TYPE_EXTERNAL_UPLEVEL_DOMAIN 0x00000008
#define DSOP_SCOPE_TYPE_EXTERNAL_DOWNLEVEL_DOMAIN 0x00000010
#define DSOP_SCOPE_TYPE_WORKGROUP 0x00000020
#define DSOP_SCOPE_TYPE_UPLEVEL_JOINED_DOMAIN 0x00000040
#define DSOP_SCOPE_TYPE_DOWNLEVEL_JOINED_DOMAIN 0x00000080
#define DSOP_FILTER_COMPUTERS 0x00000001
#define DSOP_DOWNLEVEL_FILTER_COMPUTERS 0x00000001
extern GUID CLSID_DsObjectPicker;
extern GUID IID_IDsObjectPicker;
#endif
