#include "shlobj.h"
#include "commctrl.h"
#include "string.h"

const IID IID_IShellView={0x000214e3,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IExplorerBrowser={0xdfd3b6b5,0xc10c,0x4be9,{0x85,0xf6,0xa6,0x69,0x69,0xf4,0x02,0xf6}};
const IID IID_IExplorerBrowserEvents={0x361bbdc7,0xe6ee,0x4e13,{0xbe,0x58,0x58,0xe2,0x24,0x0c,0x81,0x0f}};
const CLSID CLSID_ExplorerBrowser={0x71f96385,0xddd6,0x48d3,{0xa0,0xc1,0xae,0x06,0xe8,0xb0,0x55,0xfb}};

typedef struct browser BROWSER;
typedef struct { IShellView iface; BROWSER *browser; } BROWSER_VIEW;
struct browser {
    IExplorerBrowser iface;
    ULONG refs;
    HWND parent;
    HWND view_window;
    RECT rect;
    FOLDERSETTINGS settings;
    EXPLORER_BROWSER_OPTIONS options;
    LPITEMIDLIST current;
    IExplorerBrowserEvents *events;
    DWORD event_cookie;
    BROWSER_VIEW view;
};

static void browser_add_item(BROWSER *b,LPCWSTR text)
{
    LVITEMW item;
    if(!b->view_window)return;
    memset(&item,0,sizeof(item));
    item.mask=LVIF_TEXT;
    item.iItem=(int)SendMessageW(b->view_window,LVM_GETITEMCOUNT,0,0);
    item.pszText=(LPWSTR)text;
    SendMessageW(b->view_window,LVM_INSERTITEMW,0,(LPARAM)&item);
}

static void browser_populate(BROWSER *b)
{
    WCHAR path[MAX_PATH];
    if(!b->view_window)return;
    SendMessageW(b->view_window,LVM_DELETEALLITEMS,0,0);
    path[0]=0;
    if(b->current)SHGetPathFromIDListW(b->current,path);
    if(!path[0] || (path[0]=='/' && !path[1]))
    {
        browser_add_item(b,L"SYSTEM32");
    }
    else if((path[0]=='/' || path[0]=='\\') &&
            (path[1]=='S' || path[1]=='s'))
    {
        browser_add_item(b,L"CMD.EXE");
        browser_add_item(b,L"EXPLORER.EXE");
        browser_add_item(b,L"NOTEPAD.EXE");
        browser_add_item(b,L"CONTROL.EXE");
    }
    else browser_add_item(b,L"This folder is empty.");
    InvalidateRect(b->view_window,NULL,TRUE);
}

static HRESULT WINAPI view_qi(IShellView *iface,REFIID iid,void **out){BROWSER_VIEW*v=(BROWSER_VIEW*)iface;if(!out)return E_POINTER;*out=0;if(!IsEqualIID(iid,&IID_IUnknown)&&!IsEqualIID(iid,&IID_IShellView))return E_NOINTERFACE;*out=iface;v->browser->refs++;return S_OK;}
static ULONG WINAPI view_add(IShellView *iface){return ++((BROWSER_VIEW*)iface)->browser->refs;}
static ULONG WINAPI browser_release(IExplorerBrowser*);
static ULONG WINAPI view_release(IShellView *iface){return browser_release(&((BROWSER_VIEW*)iface)->browser->iface);}
static HRESULT WINAPI view_window(IShellView *iface,HWND*out){if(!out)return E_POINTER;*out=((BROWSER_VIEW*)iface)->browser->view_window;return S_OK;}
static HRESULT WINAPI view_help(IShellView*i,BOOL b){(void)i;(void)b;return S_OK;}
static HRESULT WINAPI view_accel(IShellView*i,MSG*m){(void)i;(void)m;return S_FALSE;}
static HRESULT WINAPI view_activate(IShellView*i,UINT n){(void)i;(void)n;return S_OK;}
static HRESULT WINAPI view_refresh(IShellView*i){browser_populate(((BROWSER_VIEW*)i)->browser);return S_OK;}
static HRESULT WINAPI view_create(IShellView*i,IShellView*p,FOLDERSETTINGS*f,void*b,RECT*r,HWND*h){(void)p;(void)b;if(f)((BROWSER_VIEW*)i)->browser->settings=*f;if(r)((BROWSER_VIEW*)i)->browser->rect=*r;if(h)*h=((BROWSER_VIEW*)i)->browser->view_window;return S_OK;}
static HRESULT WINAPI view_simple(IShellView*i){(void)i;return S_OK;}
static HRESULT WINAPI view_info(IShellView*i,FOLDERSETTINGS*f){if(!f)return E_POINTER;*f=((BROWSER_VIEW*)i)->browser->settings;return S_OK;}
static HRESULT WINAPI view_pages(IShellView*i,DWORD d,void*c,LPARAM l){(void)i;(void)d;(void)c;(void)l;return S_OK;}
static HRESULT WINAPI view_select(IShellView*i,LPCITEMIDLIST p,SVSIF f){(void)i;(void)p;(void)f;return S_OK;}
static HRESULT WINAPI view_item(IShellView*i,UINT u,REFIID r,void**o){(void)i;(void)u;(void)r;if(o)*o=0;return E_NOINTERFACE;}
static const IShellViewVtbl view_vtbl={view_qi,view_add,view_release,view_window,view_help,view_accel,view_help,view_activate,view_refresh,view_create,view_simple,view_info,view_pages,view_simple,view_select,view_item};

static HRESULT WINAPI browser_qi(IExplorerBrowser *iface,REFIID iid,void **out){BROWSER*b=(BROWSER*)iface;if(!out)return E_POINTER;*out=0;if(!IsEqualIID(iid,&IID_IUnknown)&&!IsEqualIID(iid,&IID_IExplorerBrowser))return E_NOINTERFACE;*out=iface;b->refs++;return S_OK;}
static ULONG WINAPI browser_add(IExplorerBrowser *iface){return ++((BROWSER*)iface)->refs;}
static ULONG WINAPI browser_release(IExplorerBrowser *iface){BROWSER*b=(BROWSER*)iface;ULONG refs=--b->refs;if(!refs){if(b->events)IExplorerBrowserEvents_Release(b->events);ILFree(b->current);CoTaskMemFree(b);}return refs;}
static HRESULT WINAPI browser_init(IExplorerBrowser*i,HWND h,const RECT*r,const FOLDERSETTINGS*f){BROWSER*b=(BROWSER*)i;LVCOLUMNW column;b->parent=h;if(r)b->rect=*r;if(f)b->settings=*f;b->view_window=CreateWindowExW(WS_EX_CLIENTEDGE,L"SysListView32",L"",WS_CHILD|WS_VISIBLE|LVS_REPORT,b->rect.left,b->rect.top,b->rect.right-b->rect.left,b->rect.bottom-b->rect.top,h,NULL,NULL,NULL);if(!b->view_window)return E_FAIL;memset(&column,0,sizeof(column));column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=b->rect.right-b->rect.left;column.pszText=L"Name";SendMessageW(b->view_window,LVM_INSERTCOLUMNW,0,(LPARAM)&column);return S_OK;}
static HRESULT WINAPI browser_destroy(IExplorerBrowser*i){BROWSER*b=(BROWSER*)i;if(b->view_window)DestroyWindow(b->view_window);b->view_window=0;b->parent=0;if(b->events){IExplorerBrowserEvents_Release(b->events);b->events=0;}return S_OK;}
static HRESULT WINAPI browser_rect(IExplorerBrowser*i,HDWP*d,RECT r){BROWSER*b=(BROWSER*)i;(void)d;b->rect=r;if(b->view_window)MoveWindow(b->view_window,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);return S_OK;}
static HRESULT WINAPI browser_text(IExplorerBrowser*i,LPCWSTR s){(void)i;(void)s;return S_OK;}
static HRESULT WINAPI browser_settings(IExplorerBrowser*i,const FOLDERSETTINGS*f){if(!f)return E_POINTER;((BROWSER*)i)->settings=*f;return S_OK;}
static HRESULT WINAPI browser_advise(IExplorerBrowser*i,IExplorerBrowserEvents*e,DWORD*c){BROWSER*b=(BROWSER*)i;if(!e||!c)return E_INVALIDARG;if(b->events)IExplorerBrowserEvents_Release(b->events);b->events=e;IExplorerBrowserEvents_AddRef(e);b->event_cookie=1;*c=1;return S_OK;}
static HRESULT WINAPI browser_unadvise(IExplorerBrowser*i,DWORD c){BROWSER*b=(BROWSER*)i;if(c!=b->event_cookie)return E_INVALIDARG;if(b->events)IExplorerBrowserEvents_Release(b->events);b->events=0;b->event_cookie=0;return S_OK;}
static HRESULT WINAPI browser_options(IExplorerBrowser*i,EXPLORER_BROWSER_OPTIONS o){((BROWSER*)i)->options=o;return S_OK;}
static HRESULT WINAPI browser_get_options(IExplorerBrowser*i,EXPLORER_BROWSER_OPTIONS*o){if(!o)return E_POINTER;*o=((BROWSER*)i)->options;return S_OK;}
static HRESULT browser_navigate(BROWSER*b,LPCITEMIDLIST pidl){LPITEMIDLIST copy;if(!pidl)return E_INVALIDARG;if(b->events&&FAILED(b->events->lpVtbl->OnNavigationPending(b->events,pidl)))return E_FAIL;copy=ILClone(pidl);if(!copy)return E_OUTOFMEMORY;ILFree(b->current);b->current=copy;browser_populate(b);if(b->events){b->events->lpVtbl->OnViewCreated(b->events,&b->view.iface);b->events->lpVtbl->OnNavigationComplete(b->events,b->current);}return S_OK;}
static HRESULT WINAPI browser_pidl(IExplorerBrowser*i,PCIDLIST_ABSOLUTE p,UINT f){BROWSER*b=(BROWSER*)i;(void)f;if(!p)return E_INVALIDARG;return browser_navigate(b,p);}
static HRESULT WINAPI browser_object(IExplorerBrowser*i,IUnknown*o,UINT f){BROWSER*b=(BROWSER*)i;LPITEMIDLIST p=0;HRESULT hr;(void)f;if(!o)return b->current?S_OK:E_INVALIDARG;hr=SHGetIDListFromObject(o,&p);if(SUCCEEDED(hr)){hr=browser_navigate(b,p);ILFree(p);}return hr;}
static HRESULT WINAPI browser_fill(IExplorerBrowser*i,IUnknown*o,DWORD f){(void)i;(void)o;(void)f;return S_OK;}
static HRESULT WINAPI browser_remove(IExplorerBrowser*i){BROWSER*b=(BROWSER*)i;ILFree(b->current);b->current=0;return S_OK;}
static HRESULT WINAPI browser_view(IExplorerBrowser*i,REFIID iid,void**out){return view_qi(&((BROWSER*)i)->view.iface,iid,out);}
static const IExplorerBrowserVtbl browser_vtbl={browser_qi,browser_add,browser_release,browser_init,browser_destroy,browser_rect,browser_text,browser_text,browser_settings,browser_advise,browser_unadvise,browser_options,browser_get_options,browser_pidl,browser_object,browser_fill,browser_remove,browser_view};

HRESULT WINAPI ShellCreateExplorerBrowser(REFIID iid,void **out){BROWSER*b;HRESULT hr;if(!out)return E_POINTER;*out=0;b=CoTaskMemAlloc(sizeof(*b));if(!b)return E_OUTOFMEMORY;memset(b,0,sizeof(*b));b->iface.lpVtbl=&browser_vtbl;b->refs=1;b->settings.ViewMode=FVM_DETAILS;b->settings.fFlags=FWF_AUTOARRANGE;b->view.iface.lpVtbl=&view_vtbl;b->view.browser=b;hr=browser_qi(&b->iface,iid,out);browser_release(&b->iface);return hr;}
