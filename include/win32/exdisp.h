#ifndef DISCOUNT_EXDISP_H
#define DISCOUNT_EXDISP_H
#include "oaidl.h"
typedef LONG SHANDLE_PTR;
typedef enum { READYSTATE_UNINITIALIZED=0,READYSTATE_LOADING=1,READYSTATE_LOADED=2,READYSTATE_INTERACTIVE=3,READYSTATE_COMPLETE=4 } READYSTATE;
typedef LONG OLECMDID;
typedef DWORD OLECMDF;
typedef LONG OLECMDEXECOPT;
typedef struct IWebBrowser2 IWebBrowser2;
typedef struct IWebBrowser2Vtbl {
 HRESULT(WINAPI*QueryInterface)(IWebBrowser2*,REFIID,void**); ULONG(WINAPI*AddRef)(IWebBrowser2*); ULONG(WINAPI*Release)(IWebBrowser2*);
 HRESULT(WINAPI*GetTypeInfoCount)(); HRESULT(WINAPI*GetTypeInfo)(); HRESULT(WINAPI*GetIDsOfNames)(); HRESULT(WINAPI*Invoke)(IWebBrowser2*,DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);
 HRESULT(WINAPI*GoBack)(); HRESULT(WINAPI*GoForward)(); HRESULT(WINAPI*GoHome)(); HRESULT(WINAPI*GoSearch)(); HRESULT(WINAPI*Navigate)(); HRESULT(WINAPI*Refresh)(); HRESULT(WINAPI*Refresh2)(); HRESULT(WINAPI*Stop)();
 HRESULT(WINAPI*get_Application)(); HRESULT(WINAPI*get_Parent)(); HRESULT(WINAPI*get_Container)(); HRESULT(WINAPI*get_Document)(); HRESULT(WINAPI*get_TopLevelContainer)(); HRESULT(WINAPI*get_Type)();
 HRESULT(WINAPI*get_Left)(); HRESULT(WINAPI*put_Left)(); HRESULT(WINAPI*get_Top)(); HRESULT(WINAPI*put_Top)(); HRESULT(WINAPI*get_Width)(); HRESULT(WINAPI*put_Width)(); HRESULT(WINAPI*get_Height)(); HRESULT(WINAPI*put_Height)();
 HRESULT(WINAPI*get_LocationName)(); HRESULT(WINAPI*get_LocationURL)(); HRESULT(WINAPI*get_Busy)(); HRESULT(WINAPI*Quit)(); HRESULT(WINAPI*ClientToWindow)(); HRESULT(WINAPI*PutProperty)(); HRESULT(WINAPI*GetProperty)();
 HRESULT(WINAPI*get_Name)(); HRESULT(WINAPI*get_HWND)(); HRESULT(WINAPI*get_FullName)(); HRESULT(WINAPI*get_Path)(); HRESULT(WINAPI*get_Visible)(); HRESULT(WINAPI*put_Visible)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_StatusBar)(); HRESULT(WINAPI*put_StatusBar)(IWebBrowser2*,VARIANT_BOOL);
 HRESULT(WINAPI*get_StatusText)(); HRESULT(WINAPI*put_StatusText)(); HRESULT(WINAPI*get_ToolBar)(); HRESULT(WINAPI*put_ToolBar)(); HRESULT(WINAPI*get_MenuBar)(); HRESULT(WINAPI*put_MenuBar)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_FullScreen)(); HRESULT(WINAPI*put_FullScreen)(IWebBrowser2*,VARIANT_BOOL);
 HRESULT(WINAPI*Navigate2)(); HRESULT(WINAPI*QueryStatusWB)(); HRESULT(WINAPI*ExecWB)(); HRESULT(WINAPI*ShowBrowserBar)(); HRESULT(WINAPI*get_ReadyState)(); HRESULT(WINAPI*get_Offline)(); HRESULT(WINAPI*put_Offline)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_Silent)(); HRESULT(WINAPI*put_Silent)(IWebBrowser2*,VARIANT_BOOL);
 HRESULT(WINAPI*get_RegisterAsBrowser)(); HRESULT(WINAPI*put_RegisterAsBrowser)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_RegisterAsDropTarget)(); HRESULT(WINAPI*put_RegisterAsDropTarget)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_TheaterMode)(); HRESULT(WINAPI*put_TheaterMode)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_AddressBar)(); HRESULT(WINAPI*put_AddressBar)(IWebBrowser2*,VARIANT_BOOL); HRESULT(WINAPI*get_Resizable)(); HRESULT(WINAPI*put_Resizable)(IWebBrowser2*,VARIANT_BOOL);
} IWebBrowser2Vtbl;
struct IWebBrowser2 { const IWebBrowser2Vtbl *lpVtbl; };
#define IWebBrowser2_QueryInterface(p,a,b) ((p)->lpVtbl->QueryInterface((p),(a),(b)))
#define IWebBrowser2_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IWebBrowser2_Release(p) ((p)->lpVtbl->Release((p)))
extern const IID IID_IWebBrowser,IID_IWebBrowserApp,IID_IWebBrowser2;
extern const GUID LIBID_SHDocVw;
#endif
