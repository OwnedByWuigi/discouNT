#include "shlobj.h"
#include "string.h"

const IID IID_IDispatch={0x00020400,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IShellWindows={0x85cb6900,0x4d95,0x11cf,{0x96,0x0c,0x00,0x80,0xc7,0xf4,0xee,0x85}};
const CLSID CLSID_ShellWindows={0x9ba05972,0xf6a8,0x11cf,{0xa4,0x42,0x00,0xa0,0xc9,0x0a,0x8f,0x39}};

#define SHELL_WINDOW_MAX 32
typedef struct { LONG cookie,hwnd; int window_class; LPITEMIDLIST pidl; } SHELL_WINDOW_ENTRY;
typedef struct { IShellWindows iface; ULONG refs; LONG next_cookie; SHELL_WINDOW_ENTRY entries[SHELL_WINDOW_MAX]; } SHELL_WINDOWS;

static HRESULT WINAPI sw_qi(IShellWindows*i,REFIID id,void**out){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;if(!out)return E_POINTER;*out=0;if(!IsEqualIID(id,&IID_IUnknown)&&!IsEqualIID(id,&IID_IDispatch)&&!IsEqualIID(id,&IID_IShellWindows))return E_NOINTERFACE;*out=i;s->refs++;return S_OK;}
static ULONG WINAPI sw_add(IShellWindows*i){return ++((SHELL_WINDOWS*)i)->refs;}
static ULONG WINAPI sw_release(IShellWindows*i){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;ULONG r=--s->refs;if(!r){for(int n=0;n<SHELL_WINDOW_MAX;n++)ILFree(s->entries[n].pidl);CoTaskMemFree(s);}return r;}
static HRESULT WINAPI sw_tic(IShellWindows*i,UINT*c){(void)i;if(!c)return E_POINTER;*c=0;return S_OK;}
static HRESULT WINAPI sw_ti(IShellWindows*i,UINT n,LCID l,ITypeInfo**t){(void)i;(void)n;(void)l;if(t)*t=0;return E_NOINTERFACE;}
static HRESULT WINAPI sw_names(IShellWindows*i,REFIID r,LPOLESTR*n,UINT c,LCID l,DISPID*d){(void)i;(void)r;(void)n;(void)c;(void)l;(void)d;return E_FAIL;}
static HRESULT WINAPI sw_invoke(IShellWindows*i,DISPID d,REFIID r,LCID l,WORD f,DISPPARAMS*p,VARIANT*v,EXCEPINFO*e,UINT*a){(void)i;(void)d;(void)r;(void)l;(void)f;(void)p;(void)v;(void)e;(void)a;return E_FAIL;}
static HRESULT WINAPI sw_count(IShellWindows*i,LONG*out){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;LONG c=0;if(!out)return E_POINTER;for(int n=0;n<SHELL_WINDOW_MAX;n++)if(s->entries[n].cookie)c++;*out=c;return S_OK;}
static HRESULT WINAPI sw_item(IShellWindows*i,VARIANT v,IDispatch**o){(void)i;(void)v;if(o)*o=0;return E_NOINTERFACE;}
static HRESULT WINAPI sw_enum(IShellWindows*i,IUnknown**o){(void)i;if(o)*o=0;return E_NOINTERFACE;}
static HRESULT WINAPI sw_register(IShellWindows*i,IDispatch*d,LONG hwnd,int cls,LONG*cookie){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;(void)d;if(!hwnd||!cookie)return E_POINTER;for(int n=0;n<SHELL_WINDOW_MAX;n++)if(!s->entries[n].cookie){s->entries[n].cookie=++s->next_cookie;s->entries[n].hwnd=hwnd;s->entries[n].window_class=cls;*cookie=s->entries[n].cookie;return S_OK;}return E_OUTOFMEMORY;}
static HRESULT WINAPI sw_pending(IShellWindows*i,LONG t,VARIANT*l,VARIANT*r,int c,LONG*k){(void)i;(void)t;(void)l;(void)r;(void)c;(void)k;return E_FAIL;}
static HRESULT WINAPI sw_revoke(IShellWindows*i,LONG cookie){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;for(int n=0;n<SHELL_WINDOW_MAX;n++)if(s->entries[n].cookie==cookie){ILFree(s->entries[n].pidl);memset(&s->entries[n],0,sizeof(s->entries[n]));return S_OK;}return S_FALSE;}
static HRESULT WINAPI sw_navigate(IShellWindows*i,LONG cookie,VARIANT*v){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;LPITEMIDLIST copy;ULONG bytes;if(!v||V_VT(v)!=(VT_ARRAY|VT_UI1)||!V_ARRAY(v)||!V_ARRAY(v)->pvData)return E_INVALIDARG;bytes=V_ARRAY(v)->rgsabound[0].cElements;copy=CoTaskMemAlloc(bytes);if(!copy)return E_OUTOFMEMORY;memcpy(copy,V_ARRAY(v)->pvData,bytes);for(int n=0;n<SHELL_WINDOW_MAX;n++)if(s->entries[n].cookie==cookie){ILFree(s->entries[n].pidl);s->entries[n].pidl=copy;return S_OK;}CoTaskMemFree(copy);return E_INVALIDARG;}
static HRESULT WINAPI sw_active(IShellWindows*i,LONG c,VARIANT_BOOL a){(void)i;(void)c;(void)a;return S_OK;}
static HRESULT WINAPI sw_find(IShellWindows*i,VARIANT*l,VARIANT*r,int cls,LONG*hwnd,int options,IDispatch**disp){SHELL_WINDOWS*s=(SHELL_WINDOWS*)i;(void)r;(void)options;if(!hwnd)return E_POINTER;*hwnd=0;if(disp)*disp=0;for(int n=0;n<SHELL_WINDOW_MAX;n++){SHELL_WINDOW_ENTRY*e=&s->entries[n];if(!e->cookie||e->window_class!=cls)continue;if(l&&V_VT(l)==(VT_ARRAY|VT_UI1)&&V_ARRAY(l)&&e->pidl&&!ILIsEqual((LPCITEMIDLIST)V_ARRAY(l)->pvData,e->pidl))continue;*hwnd=e->hwnd;return S_OK;}return S_FALSE;}
static HRESULT WINAPI sw_created(IShellWindows*i,LONG c,IUnknown*p){(void)i;(void)c;(void)p;return S_OK;}
static HRESULT WINAPI sw_attach(IShellWindows*i,VARIANT_BOOL a){(void)i;(void)a;return S_OK;}
static const IShellWindowsVtbl sw_vtbl={sw_qi,sw_add,sw_release,sw_tic,sw_ti,sw_names,sw_invoke,sw_count,sw_item,sw_enum,sw_register,sw_pending,sw_revoke,sw_navigate,sw_active,sw_find,sw_created,sw_attach};

HRESULT WINAPI ShellCreateShellWindows(REFIID iid,void **out){SHELL_WINDOWS*s;HRESULT hr;if(!out)return E_POINTER;*out=0;s=CoTaskMemAlloc(sizeof(*s));if(!s)return E_OUTOFMEMORY;memset(s,0,sizeof(*s));s->iface.lpVtbl=&sw_vtbl;s->refs=1;hr=sw_qi(&s->iface,iid,out);sw_release(&s->iface);return hr;}
