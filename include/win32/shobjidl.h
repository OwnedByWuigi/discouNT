#ifndef DISCOUNT_SHOBJIDL_H
#define DISCOUNT_SHOBJIDL_H
#include "ole2.h"
#include "oaidl.h"

typedef ULONG SFGAOF;
typedef struct _SHITEMID { USHORT cb; BYTE abID[1]; } SHITEMID;
typedef struct _ITEMIDLIST { SHITEMID mkid; } ITEMIDLIST;
typedef ITEMIDLIST *LPITEMIDLIST, *PIDLIST_ABSOLUTE, *PITEMID_CHILD;
typedef const ITEMIDLIST *LPCITEMIDLIST, *PCIDLIST_ABSOLUTE, *PCUITEMID_CHILD;
typedef const ITEMIDLIST *LPCITEMIDLIST;
typedef struct _STRRET { UINT uType; union { LPWSTR pOleStr; UINT uOffset; char cStr[MAX_PATH]; }; } STRRET;
#define STRRET_WSTR 0
#define STRRET_CSTR 2
#define SHCONTF_FOLDERS 0x20
#define SHCONTF_NONFOLDERS 0x40
#define SFGAO_FOLDER 0x20000000
#define SFGAO_FILESYSTEM 0x40000000
#define SFGAO_BROWSABLE 0x08000000
#define SHGDN_INFOLDER 1
#define SHGDN_FORADDRESSBAR 0x4000

typedef struct IBindCtx IBindCtx;
typedef struct IShellFolder IShellFolder;
typedef struct IEnumIDList IEnumIDList;
typedef struct IShellView IShellView;
typedef struct IExplorerBrowser IExplorerBrowser;
typedef struct IExplorerBrowserEvents IExplorerBrowserEvents;
typedef struct IShellWindows IShellWindows;
typedef struct IServiceProvider IServiceProvider;
typedef struct IShellBrowser IShellBrowser;
typedef struct IShellLinkW IShellLinkW;
typedef struct IPersistFile IPersistFile;
typedef struct IStream IStream;
typedef HANDLE HOLEMENU;
typedef struct { LONG width[6]; } OLEMENUGROUPWIDTHS;
typedef OLECHAR *LPCOLESTR;
typedef struct { int iBitmap,idCommand; BYTE fsState,fsStyle; BYTE reserved[2]; DWORD_PTR dwData; INT_PTR iString; } TBBUTTONSB,*LPTBBUTTONSB;
typedef struct IPersistFolder2 IPersistFolder2;

typedef struct IEnumIDListVtbl {
 HRESULT(WINAPI*QueryInterface)(IEnumIDList*,REFIID,void**); ULONG(WINAPI*AddRef)(IEnumIDList*); ULONG(WINAPI*Release)(IEnumIDList*);
 HRESULT(WINAPI*Next)(IEnumIDList*,ULONG,LPITEMIDLIST*,ULONG*); HRESULT(WINAPI*Skip)(IEnumIDList*,ULONG); HRESULT(WINAPI*Reset)(IEnumIDList*); HRESULT(WINAPI*Clone)(IEnumIDList*,IEnumIDList**);
} IEnumIDListVtbl;
struct IEnumIDList { const IEnumIDListVtbl *lpVtbl; };
#define IEnumIDList_Next(p,a,b,c) ((p)->lpVtbl->Next((p),(a),(b),(c)))
#define IEnumIDList_Release(p) ((p)->lpVtbl->Release((p)))

typedef struct IShellFolderVtbl {
 HRESULT(WINAPI*QueryInterface)(IShellFolder*,REFIID,void**); ULONG(WINAPI*AddRef)(IShellFolder*); ULONG(WINAPI*Release)(IShellFolder*);
 HRESULT(WINAPI*ParseDisplayName)(IShellFolder*,HWND,IBindCtx*,LPWSTR,ULONG*,LPITEMIDLIST*,ULONG*);
 HRESULT(WINAPI*EnumObjects)(IShellFolder*,HWND,DWORD,IEnumIDList**);
 HRESULT(WINAPI*BindToObject)(IShellFolder*,LPCITEMIDLIST,IBindCtx*,REFIID,void**);
 HRESULT(WINAPI*BindToStorage)(IShellFolder*,LPCITEMIDLIST,IBindCtx*,REFIID,void**);
 HRESULT(WINAPI*CompareIDs)(IShellFolder*,LPARAM,LPCITEMIDLIST,LPCITEMIDLIST);
 HRESULT(WINAPI*CreateViewObject)(IShellFolder*,HWND,REFIID,void**);
 HRESULT(WINAPI*GetAttributesOf)(IShellFolder*,UINT,LPCITEMIDLIST*,SFGAOF*);
 HRESULT(WINAPI*GetUIObjectOf)(IShellFolder*,HWND,UINT,LPCITEMIDLIST*,REFIID,UINT*,void**);
 HRESULT(WINAPI*GetDisplayNameOf)(IShellFolder*,LPCITEMIDLIST,DWORD,STRRET*);
 HRESULT(WINAPI*SetNameOf)(IShellFolder*,HWND,LPCITEMIDLIST,LPCWSTR,DWORD,LPITEMIDLIST*);
} IShellFolderVtbl;
struct IShellFolder { const IShellFolderVtbl *lpVtbl; };
#define IShellFolder_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((p),(a),(b)))
#define IShellFolder_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IShellFolder_Release(p) ((p)->lpVtbl->Release((p)))
#define IShellFolder_ParseDisplayName(p,a,b,c,d,e,f) ((p)->lpVtbl->ParseDisplayName((p),(a),(b),(c),(d),(e),(f)))
#define IShellFolder_EnumObjects(p,a,b,c) ((p)->lpVtbl->EnumObjects((p),(a),(b),(c)))
#define IShellFolder_BindToObject(p,a,b,c,d) ((p)->lpVtbl->BindToObject((p),(a),(b),(c),(d)))
#define IShellFolder_CreateViewObject(p,a,b,c) ((p)->lpVtbl->CreateViewObject((p),(a),(b),(c)))
#define IShellFolder_GetAttributesOf(p,a,b,c) ((p)->lpVtbl->GetAttributesOf((p),(a),(b),(c)))
#define IShellFolder_GetDisplayNameOf(p,a,b,c) ((p)->lpVtbl->GetDisplayNameOf((p),(a),(b),(c)))

typedef struct IPersistFolder2Vtbl {
 HRESULT(WINAPI*QueryInterface)(IPersistFolder2*,REFIID,void**); ULONG(WINAPI*AddRef)(IPersistFolder2*); ULONG(WINAPI*Release)(IPersistFolder2*);
 HRESULT(WINAPI*GetClassID)(IPersistFolder2*,CLSID*); HRESULT(WINAPI*Initialize)(IPersistFolder2*,LPCITEMIDLIST); HRESULT(WINAPI*GetCurFolder)(IPersistFolder2*,LPITEMIDLIST*);
} IPersistFolder2Vtbl;
struct IPersistFolder2 { const IPersistFolder2Vtbl *lpVtbl; };
#define IPersistFolder2_GetCurFolder(p,a) ((p)->lpVtbl->GetCurFolder((p),(a)))
#define IPersistFolder2_Release(p) ((p)->lpVtbl->Release((p)))

typedef UINT FOLDERVIEWMODE;
typedef UINT FOLDERFLAGS;
typedef UINT EXPLORER_BROWSER_OPTIONS;
typedef UINT SVSIF;
typedef struct FOLDERSETTINGS { FOLDERVIEWMODE ViewMode; FOLDERFLAGS fFlags; } FOLDERSETTINGS;

#define FVM_DETAILS 4
#define FWF_AUTOARRANGE 0x00000001
#define EBO_SHOWFRAMES 0x00000001
#define SBSP_ABSOLUTE 0x0000
#define SBSP_PARENT 0x2000
#define SBSP_NAVIGATEBACK 0x4000
#define SBSP_NAVIGATEFORWARD 0x8000
#define SVSI_SELECT 0x00000001
#define SVSI_EDIT 0x00000003
#define SVSI_DESELECTOTHERS 0x00000004
#define SVSI_ENSUREVISIBLE 0x00000008
#define SVSI_FOCUSED 0x00000010
#define OFASI_EDIT 0x0001

typedef struct IShellViewVtbl {
 HRESULT(WINAPI*QueryInterface)(IShellView*,REFIID,void**); ULONG(WINAPI*AddRef)(IShellView*); ULONG(WINAPI*Release)(IShellView*);
 HRESULT(WINAPI*GetWindow)(IShellView*,HWND*); HRESULT(WINAPI*ContextSensitiveHelp)(IShellView*,BOOL);
 HRESULT(WINAPI*TranslateAccelerator)(IShellView*,MSG*); HRESULT(WINAPI*EnableModeless)(IShellView*,BOOL);
 HRESULT(WINAPI*UIActivate)(IShellView*,UINT); HRESULT(WINAPI*Refresh)(IShellView*);
 HRESULT(WINAPI*CreateViewWindow)(IShellView*,IShellView*,FOLDERSETTINGS*,void*,RECT*,HWND*);
 HRESULT(WINAPI*DestroyViewWindow)(IShellView*); HRESULT(WINAPI*GetCurrentInfo)(IShellView*,FOLDERSETTINGS*);
 HRESULT(WINAPI*AddPropertySheetPages)(IShellView*,DWORD,void*,LPARAM); HRESULT(WINAPI*SaveViewState)(IShellView*);
 HRESULT(WINAPI*SelectItem)(IShellView*,LPCITEMIDLIST,SVSIF); HRESULT(WINAPI*GetItemObject)(IShellView*,UINT,REFIID,void**);
} IShellViewVtbl;
struct IShellView { const IShellViewVtbl *lpVtbl; };
#define IShellView_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IShellView_Release(p) ((p)->lpVtbl->Release((p)))
#define IShellView_SelectItem(p,a,b) ((p)->lpVtbl->SelectItem((p),(a),(b)))

typedef struct IExplorerBrowserEventsVtbl {
 HRESULT(WINAPI*QueryInterface)(IExplorerBrowserEvents*,REFIID,void**); ULONG(WINAPI*AddRef)(IExplorerBrowserEvents*); ULONG(WINAPI*Release)(IExplorerBrowserEvents*);
 HRESULT(WINAPI*OnNavigationPending)(IExplorerBrowserEvents*,PCIDLIST_ABSOLUTE); HRESULT(WINAPI*OnViewCreated)(IExplorerBrowserEvents*,IShellView*);
 HRESULT(WINAPI*OnNavigationComplete)(IExplorerBrowserEvents*,PCIDLIST_ABSOLUTE); HRESULT(WINAPI*OnNavigationFailed)(IExplorerBrowserEvents*,PCIDLIST_ABSOLUTE);
} IExplorerBrowserEventsVtbl;
struct IExplorerBrowserEvents { const IExplorerBrowserEventsVtbl *lpVtbl; };
#define IExplorerBrowserEvents_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IExplorerBrowserEvents_Release(p) ((p)->lpVtbl->Release((p)))

typedef struct IExplorerBrowserVtbl {
 HRESULT(WINAPI*QueryInterface)(IExplorerBrowser*,REFIID,void**); ULONG(WINAPI*AddRef)(IExplorerBrowser*); ULONG(WINAPI*Release)(IExplorerBrowser*);
 HRESULT(WINAPI*Initialize)(IExplorerBrowser*,HWND,const RECT*,const FOLDERSETTINGS*); HRESULT(WINAPI*Destroy)(IExplorerBrowser*);
 HRESULT(WINAPI*SetRect)(IExplorerBrowser*,HDWP*,RECT); HRESULT(WINAPI*SetPropertyBag)(IExplorerBrowser*,LPCWSTR); HRESULT(WINAPI*SetEmptyText)(IExplorerBrowser*,LPCWSTR);
 HRESULT(WINAPI*SetFolderSettings)(IExplorerBrowser*,const FOLDERSETTINGS*); HRESULT(WINAPI*Advise)(IExplorerBrowser*,IExplorerBrowserEvents*,DWORD*); HRESULT(WINAPI*Unadvise)(IExplorerBrowser*,DWORD);
 HRESULT(WINAPI*SetOptions)(IExplorerBrowser*,EXPLORER_BROWSER_OPTIONS); HRESULT(WINAPI*GetOptions)(IExplorerBrowser*,EXPLORER_BROWSER_OPTIONS*);
 HRESULT(WINAPI*BrowseToIDList)(IExplorerBrowser*,PCIDLIST_ABSOLUTE,UINT); HRESULT(WINAPI*BrowseToObject)(IExplorerBrowser*,IUnknown*,UINT);
 HRESULT(WINAPI*FillFromObject)(IExplorerBrowser*,IUnknown*,DWORD); HRESULT(WINAPI*RemoveAll)(IExplorerBrowser*); HRESULT(WINAPI*GetCurrentView)(IExplorerBrowser*,REFIID,void**);
} IExplorerBrowserVtbl;
struct IExplorerBrowser { const IExplorerBrowserVtbl *lpVtbl; };
#define IExplorerBrowser_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IExplorerBrowser_Release(p) ((p)->lpVtbl->Release((p)))
#define IExplorerBrowser_Initialize(p,a,b,c) ((p)->lpVtbl->Initialize((p),(a),(b),(c)))
#define IExplorerBrowser_Destroy(p) ((p)->lpVtbl->Destroy((p)))
#define IExplorerBrowser_SetRect(p,a,b) ((p)->lpVtbl->SetRect((p),(a),(b)))
#define IExplorerBrowser_Advise(p,a,b) ((p)->lpVtbl->Advise((p),(a),(b)))
#define IExplorerBrowser_Unadvise(p,a) ((p)->lpVtbl->Unadvise((p),(a)))
#define IExplorerBrowser_SetOptions(p,a) ((p)->lpVtbl->SetOptions((p),(a)))
#define IExplorerBrowser_BrowseToIDList(p,a,b) ((p)->lpVtbl->BrowseToIDList((p),(a),(b)))
#define IExplorerBrowser_BrowseToObject(p,a,b) ((p)->lpVtbl->BrowseToObject((p),(a),(b)))
#define IExplorerBrowser_GetCurrentView(p,a,b) ((p)->lpVtbl->GetCurrentView((p),(a),(b)))

extern const IID IID_IShellFolder, IID_IEnumIDList, IID_IPersistFolder2, IID_IShellView;
extern const IID IID_IExplorerBrowser, IID_IExplorerBrowserEvents;
extern const CLSID CLSID_ExplorerBrowser;
HRESULT WINAPI ShellCreateExplorerBrowser(REFIID iid,void **object);

#define SWC_EXPLORER 0
#define SWC_BROWSER 1
#define SWC_3RDPARTY 2
#define SWC_CALLBACK 4
#define SWC_DESKTOP 8
#define SWFO_NEEDDISPATCH 1
typedef struct IShellWindowsVtbl {
 HRESULT(WINAPI*QueryInterface)(IShellWindows*,REFIID,void**); ULONG(WINAPI*AddRef)(IShellWindows*); ULONG(WINAPI*Release)(IShellWindows*);
 HRESULT(WINAPI*GetTypeInfoCount)(IShellWindows*,UINT*); HRESULT(WINAPI*GetTypeInfo)(IShellWindows*,UINT,LCID,ITypeInfo**);
 HRESULT(WINAPI*GetIDsOfNames)(IShellWindows*,REFIID,LPOLESTR*,UINT,LCID,DISPID*); HRESULT(WINAPI*Invoke)(IShellWindows*,DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);
 HRESULT(WINAPI*get_Count)(IShellWindows*,LONG*); HRESULT(WINAPI*Item)(IShellWindows*,VARIANT,IDispatch**); HRESULT(WINAPI*_NewEnum)(IShellWindows*,IUnknown**);
 HRESULT(WINAPI*Register)(IShellWindows*,IDispatch*,LONG,int,LONG*); HRESULT(WINAPI*RegisterPending)(IShellWindows*,LONG,VARIANT*,VARIANT*,int,LONG*);
 HRESULT(WINAPI*Revoke)(IShellWindows*,LONG); HRESULT(WINAPI*OnNavigate)(IShellWindows*,LONG,VARIANT*); HRESULT(WINAPI*OnActivated)(IShellWindows*,LONG,VARIANT_BOOL);
 HRESULT(WINAPI*FindWindowSW)(IShellWindows*,VARIANT*,VARIANT*,int,LONG*,int,IDispatch**); HRESULT(WINAPI*OnCreated)(IShellWindows*,LONG,IUnknown*); HRESULT(WINAPI*ProcessAttachDetach)(IShellWindows*,VARIANT_BOOL);
} IShellWindowsVtbl;
struct IShellWindows { const IShellWindowsVtbl *lpVtbl; };
#define IShellWindows_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IShellWindows_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((p),(a),(b)))
#define IShellWindows_Release(p) ((p)->lpVtbl->Release((p)))
#define IShellWindows_Register(p,a,b,c,d) ((p)->lpVtbl->Register((p),(a),(b),(c),(d)))
#define IShellWindows_Revoke(p,a) ((p)->lpVtbl->Revoke((p),(a)))
#define IShellWindows_OnNavigate(p,a,b) ((p)->lpVtbl->OnNavigate((p),(a),(b)))
#define IShellWindows_FindWindowSW(p,a,b,c,d,e,f) ((p)->lpVtbl->FindWindowSW((p),(a),(b),(c),(d),(e),(f)))
extern const IID IID_IShellWindows;
extern const CLSID CLSID_ShellWindows;
HRESULT WINAPI ShellCreateShellWindows(REFIID iid,void **object);

typedef struct IServiceProviderVtbl { HRESULT(WINAPI*QueryInterface)(IServiceProvider*,REFIID,void**); ULONG(WINAPI*AddRef)(IServiceProvider*); ULONG(WINAPI*Release)(IServiceProvider*); HRESULT(WINAPI*QueryService)(IServiceProvider*,REFGUID,REFIID,void**); } IServiceProviderVtbl;
struct IServiceProvider { const IServiceProviderVtbl *lpVtbl; };
typedef struct IShellBrowserVtbl {
 HRESULT(WINAPI*QueryInterface)(IShellBrowser*,REFIID,void**); ULONG(WINAPI*AddRef)(IShellBrowser*); ULONG(WINAPI*Release)(IShellBrowser*); HRESULT(WINAPI*GetWindow)(IShellBrowser*,HWND*); HRESULT(WINAPI*ContextSensitiveHelp)(IShellBrowser*,BOOL);
 HRESULT(WINAPI*InsertMenusSB)(IShellBrowser*,HMENU,OLEMENUGROUPWIDTHS*); HRESULT(WINAPI*SetMenuSB)(IShellBrowser*,HMENU,HOLEMENU,HWND); HRESULT(WINAPI*RemoveMenusSB)(IShellBrowser*,HMENU); HRESULT(WINAPI*SetStatusTextSB)(IShellBrowser*,LPCOLESTR); HRESULT(WINAPI*EnableModelessSB)(IShellBrowser*,BOOL); HRESULT(WINAPI*TranslateAcceleratorSB)(IShellBrowser*,MSG*,WORD); HRESULT(WINAPI*BrowseObject)(IShellBrowser*,LPCITEMIDLIST,UINT); HRESULT(WINAPI*GetViewStateStream)(IShellBrowser*,DWORD,IStream**); HRESULT(WINAPI*GetControlWindow)(IShellBrowser*,UINT,HWND*); HRESULT(WINAPI*SendControlMsg)(IShellBrowser*,UINT,UINT,WPARAM,LPARAM,LRESULT*); HRESULT(WINAPI*QueryActiveShellView)(IShellBrowser*,IShellView**); HRESULT(WINAPI*OnViewWindowActive)(IShellBrowser*,IShellView*); HRESULT(WINAPI*SetToolbarItems)(IShellBrowser*,LPTBBUTTONSB,UINT,UINT);
} IShellBrowserVtbl;
struct IShellBrowser { const IShellBrowserVtbl *lpVtbl; };
#define IShellBrowser_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((p),(a),(b)))
typedef struct IShellLinkWVtbl { HRESULT(WINAPI*QueryInterface)(IShellLinkW*,REFIID,void**); ULONG(WINAPI*AddRef)(IShellLinkW*); ULONG(WINAPI*Release)(IShellLinkW*); HRESULT(WINAPI*GetPath)(IShellLinkW*,LPWSTR,int,WIN32_FIND_DATAW*,DWORD); HRESULT(WINAPI*GetIDList)(); HRESULT(WINAPI*SetIDList)(); HRESULT(WINAPI*GetDescription)(); HRESULT(WINAPI*SetDescription)(); HRESULT(WINAPI*GetWorkingDirectory)(); HRESULT(WINAPI*SetWorkingDirectory)(); HRESULT(WINAPI*GetArguments)(); HRESULT(WINAPI*SetArguments)(); HRESULT(WINAPI*GetHotkey)(); HRESULT(WINAPI*SetHotkey)(); HRESULT(WINAPI*GetShowCmd)(); HRESULT(WINAPI*SetShowCmd)(); HRESULT(WINAPI*GetIconLocation)(IShellLinkW*,LPWSTR,int,int*); } IShellLinkWVtbl;
struct IShellLinkW { const IShellLinkWVtbl *lpVtbl; };
#define IShellLinkW_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((p),(a),(b)))
#define IShellLinkW_Release(p) ((p)->lpVtbl->Release((p)))
#define IShellLinkW_GetPath(p,a,b,c,d) ((p)->lpVtbl->GetPath((p),(a),(b),(c),(d)))
#define IShellLinkW_GetIconLocation(p,a,b,c) ((p)->lpVtbl->GetIconLocation((p),(a),(b),(c)))
typedef struct IPersistFileVtbl { HRESULT(WINAPI*QueryInterface)(IPersistFile*,REFIID,void**); ULONG(WINAPI*AddRef)(IPersistFile*); ULONG(WINAPI*Release)(IPersistFile*); HRESULT(WINAPI*GetClassID)(IPersistFile*,CLSID*); HRESULT(WINAPI*IsDirty)(IPersistFile*); HRESULT(WINAPI*Load)(IPersistFile*,LPCWSTR,DWORD); HRESULT(WINAPI*Save)(IPersistFile*,LPCWSTR,BOOL); HRESULT(WINAPI*SaveCompleted)(IPersistFile*,LPCWSTR); HRESULT(WINAPI*GetCurFile)(IPersistFile*,LPWSTR*); } IPersistFileVtbl;
struct IPersistFile { const IPersistFileVtbl *lpVtbl; };
#define IPersistFile_Load(p,a,b) ((p)->lpVtbl->Load((p),(a),(b)))
#define IPersistFile_Release(p) ((p)->lpVtbl->Release((p)))
#define SLGP_RAWPATH 4
extern const IID IID_IServiceProvider,IID_IShellBrowser,IID_IOleWindow,IID_IShellLinkW,IID_IPersistFile;
extern const GUID SID_STopLevelBrowser;
extern const CLSID CLSID_ShellLink;
#endif
