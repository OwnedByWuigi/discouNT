#include "shlobj.h"
#include "string.h"

const IID IID_IShellFolder={0x000214e6,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IEnumIDList={0x000214f2,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IPersistFolder2={0x1ac3d9f0,0x175c,0x11d1,{0x95,0xbe,0x00,0x60,0x97,0x97,0xea,0x4f}};
const IID IID_IServiceProvider={0x6d5140c1,0x7436,0x11ce,{0x80,0x34,0x00,0xaa,0x00,0x60,0x09,0xfa}};
const IID IID_IShellBrowser={0x000214e2,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IOleWindow={0x00000114,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IShellLinkW={0x000214f9,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IPersistFile={0x0000010b,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const GUID SID_STopLevelBrowser={0x4c96be40,0x915c,0x11cf,{0x99,0xd3,0x00,0xaa,0x00,0x4a,0xe8,0x37}};
const CLSID CLSID_ShellLink={0x00021401,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const GUID FOLDERID_Desktop={0xb4bfcc3a,0xdb2c,0x424c,{0xb0,0x29,0x7f,0xe9,0x9a,0x87,0xc6,0x41}};
const GUID FOLDERID_PublicDesktop={0xc4aa340d,0xf20f,0x4863,{0xaf,0xef,0xf8,0x7e,0xf2,0xe6,0xba,0x25}};

static UINT wlen(LPCWSTR s){UINT n=0;if(s)while(s[n])n++;return n;}
static void *zalloc(SIZE_T size){void *p=CoTaskMemAlloc(size);if(p)memset(p,0,(uint32_t)size);return p;}
static void wcopy(LPWSTR d,LPCWSTR s){while((*d++=*s++));}
static int wequal(LPCWSTR a,LPCWSTR b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int wpref(LPCWSTR a,LPCWSTR b){while(*a&&*a==*b){a++;b++;}return !*a;}
static LPCWSTR pidl_text(LPCITEMIDLIST p){return (!p||!p->mkid.cb)?L"":(LPCWSTR)p->mkid.abID;}

static LPITEMIDLIST make_pidl(LPCWSTR text){
 UINT bytes=(wlen(text)+1)*sizeof(WCHAR),total=sizeof(USHORT)+bytes+sizeof(USHORT);
 LPITEMIDLIST p=(LPITEMIDLIST)CoTaskMemAlloc(total);if(!p)return 0;
 p->mkid.cb=(USHORT)(sizeof(USHORT)+bytes);memcpy(p->mkid.abID,text,bytes);
 *(USHORT*)((BYTE*)p+p->mkid.cb)=0;return p;
}
UINT WINAPI ILGetSize(LPCITEMIDLIST p){UINT n=0;if(!p)return 0;do{n+=p->mkid.cb;if(!p->mkid.cb)break;p=(LPCITEMIDLIST)((const BYTE*)p+p->mkid.cb);}while(1);return n;}
LPITEMIDLIST WINAPI ILClone(LPCITEMIDLIST p){UINT n=ILGetSize(p);LPITEMIDLIST r;if(!n)return 0;r=CoTaskMemAlloc(n);if(r)memcpy(r,p,n);return r;}
LPITEMIDLIST WINAPI ILCloneFirst(LPCITEMIDLIST p){return p&&p->mkid.cb?make_pidl(pidl_text(p)):make_pidl(L"");}
LPCITEMIDLIST WINAPI ILGetNext(LPCITEMIDLIST p){return p&&p->mkid.cb?(LPCITEMIDLIST)((const BYTE*)p+p->mkid.cb):p;}
BOOL WINAPI ILIsEmpty(LPCITEMIDLIST p){return !p||!p->mkid.cb;}
void WINAPI ILFree(LPITEMIDLIST p){CoTaskMemFree(p);}
BOOL WINAPI ILIsEqual(LPCITEMIDLIST a,LPCITEMIDLIST b){return wequal(pidl_text(a),pidl_text(b));}
BOOL WINAPI ILIsParent(LPCITEMIDLIST a,LPCITEMIDLIST b,BOOL immediate){LPCWSTR x=pidl_text(a),y=pidl_text(b);UINT n=wlen(x);if(!wpref(x,y)||y[n]==0)return FALSE;if(immediate){y+=n;if(*y=='/'||*y=='\\')y++;while(*y)if(*y++=='/'||y[-1]=='\\')return FALSE;}return TRUE;}
LPITEMIDLIST WINAPI ILFindChild(LPCITEMIDLIST a,LPCITEMIDLIST b){LPCWSTR x=pidl_text(a),y=pidl_text(b);UINT n=wlen(x);if(!wpref(x,y))return 0;y+=n;if(*y=='/'||*y=='\\')y++;return make_pidl(y);}
LPITEMIDLIST WINAPI ILCreateFromPathW(LPCWSTR path){return path?make_pidl(path):0;}
LPITEMIDLIST WINAPI ILCombine(LPCITEMIDLIST a,LPCITEMIDLIST b){
 WCHAR path[MAX_PATH];LPCWSTR x=pidl_text(a),y=pidl_text(b);UINT n=0;
 while(*x&&n<MAX_PATH-1)path[n++]=*x++;if(n&&path[n-1]!='/'&&path[n-1]!='\\'&&*y)path[n++]='/';
 while(*y&&n<MAX_PATH-1)path[n++]=*y++;path[n]=0;return make_pidl(path);
}
BOOL WINAPI SHGetPathFromIDListW(LPCITEMIDLIST p,LPWSTR path){if(!p||!path)return FALSE;wcopy(path,pidl_text(p));return TRUE;}

typedef struct { IEnumIDList iface; ULONG refs,index,count; LPCWSTR names[4]; } ENUM_LIST;
static HRESULT WINAPI enum_qi(IEnumIDList*p,REFIID i,void**o){if(!o)return E_POINTER;*o=0;if(IsEqualIID(i,&IID_IUnknown)||IsEqualIID(i,&IID_IEnumIDList)){*o=p;p->lpVtbl->AddRef(p);return S_OK;}return E_NOINTERFACE;}
static ULONG WINAPI enum_add(IEnumIDList*p){return ++((ENUM_LIST*)p)->refs;}
static ULONG WINAPI enum_rel(IEnumIDList*p){ENUM_LIST*e=(ENUM_LIST*)p;ULONG r=--e->refs;if(!r)CoTaskMemFree(e);return r;}
static HRESULT WINAPI enum_next(IEnumIDList*p,ULONG n,LPITEMIDLIST*out,ULONG*fetched){ENUM_LIST*e=(ENUM_LIST*)p;ULONG got=0;if(!out)return E_POINTER;while(got<n&&e->index<e->count)out[got++]=make_pidl(e->names[e->index++]);if(fetched)*fetched=got;return got==n?S_OK:S_FALSE;}
static HRESULT WINAPI enum_skip(IEnumIDList*p,ULONG n){ENUM_LIST*e=(ENUM_LIST*)p;e->index+=n;if(e->index>e->count)e->index=e->count;return e->index<e->count?S_OK:S_FALSE;}
static HRESULT WINAPI enum_reset(IEnumIDList*p){((ENUM_LIST*)p)->index=0;return S_OK;}
static HRESULT WINAPI enum_clone(IEnumIDList*p,IEnumIDList**o){(void)p;(void)o;return E_FAIL;}
static const IEnumIDListVtbl enum_vtbl={enum_qi,enum_add,enum_rel,enum_next,enum_skip,enum_reset,enum_clone};

typedef struct { IShellFolder folder; IPersistFolder2 persist; ULONG refs; WCHAR path[MAX_PATH]; } SHELL_FOLDER;
static SHELL_FOLDER *from_persist(IPersistFolder2*p){return (SHELL_FOLDER*)((BYTE*)p-sizeof(IShellFolder));}
static HRESULT folder_create(LPCWSTR path,REFIID iid,void **out);
static HRESULT WINAPI folder_qi(IShellFolder*p,REFIID i,void**o){SHELL_FOLDER*f=(SHELL_FOLDER*)p;if(!o)return E_POINTER;*o=0;if(IsEqualIID(i,&IID_IUnknown)||IsEqualIID(i,&IID_IShellFolder))*o=&f->folder;else if(IsEqualIID(i,&IID_IPersistFolder2))*o=&f->persist;else return E_NOINTERFACE;f->refs++;return S_OK;}
static ULONG WINAPI folder_add(IShellFolder*p){return ++((SHELL_FOLDER*)p)->refs;}
static ULONG WINAPI folder_rel(IShellFolder*p){SHELL_FOLDER*f=(SHELL_FOLDER*)p;ULONG r=--f->refs;if(!r)CoTaskMemFree(f);return r;}
static HRESULT WINAPI folder_parse(IShellFolder*p,HWND h,IBindCtx*b,LPWSTR name,ULONG*eat,LPITEMIDLIST*out,ULONG*attr){(void)p;(void)h;(void)b;if(!name||!out)return E_INVALIDARG;*out=make_pidl(name);if(eat)*eat=wlen(name);if(attr)*attr=SFGAO_FILESYSTEM|SFGAO_BROWSABLE;return *out?S_OK:E_OUTOFMEMORY;}
static HRESULT WINAPI folder_enum(IShellFolder*p,HWND h,DWORD flags,IEnumIDList**out){SHELL_FOLDER*f=(SHELL_FOLDER*)p;ENUM_LIST*e;(void)h;(void)flags;if(!out)return E_POINTER;*out=0;e=zalloc(sizeof(*e));if(!e)return E_OUTOFMEMORY;e->iface.lpVtbl=&enum_vtbl;e->refs=1;if(wequal(f->path,L"/")){e->names[e->count++]=L"DISCOUNT";}*out=&e->iface;return S_OK;}
static HRESULT WINAPI folder_bind(IShellFolder*p,LPCITEMIDLIST child,IBindCtx*b,REFIID iid,void**out){SHELL_FOLDER*f=(SHELL_FOLDER*)p;LPITEMIDLIST base=make_pidl(f->path),full;(void)b;full=ILCombine(base,child);ILFree(base);if(!full)return E_OUTOFMEMORY;HRESULT hr=folder_create(pidl_text(full),iid,out);ILFree(full);return hr;}
static HRESULT WINAPI folder_fail4(IShellFolder*p,LPCITEMIDLIST a,IBindCtx*b,REFIID i,void**o){(void)p;(void)a;(void)b;(void)i;if(o)*o=0;return E_NOINTERFACE;}
static HRESULT WINAPI folder_compare(IShellFolder*p,LPARAM l,LPCITEMIDLIST a,LPCITEMIDLIST b){(void)p;(void)l;return ILIsEqual(a,b)?S_OK:S_FALSE;}
static HRESULT WINAPI folder_view(IShellFolder*p,HWND h,REFIID i,void**o){(void)p;(void)h;(void)i;if(o)*o=0;return E_NOINTERFACE;}
static HRESULT WINAPI folder_attr(IShellFolder*p,UINT n,LPCITEMIDLIST*a,SFGAOF*f){(void)p;(void)n;(void)a;if(!f)return E_POINTER;*f&=SFGAO_FOLDER|SFGAO_FILESYSTEM|SFGAO_BROWSABLE;return S_OK;}
static HRESULT WINAPI folder_ui(IShellFolder*p,HWND h,UINT n,LPCITEMIDLIST*a,REFIID i,UINT*r,void**o){(void)p;(void)h;(void)n;(void)a;(void)i;(void)r;if(o)*o=0;return E_NOINTERFACE;}
static HRESULT WINAPI folder_name(IShellFolder*p,LPCITEMIDLIST child,DWORD flags,STRRET*out){(void)p;(void)flags;LPCWSTR s=pidl_text(child);UINT bytes=(wlen(s)+1)*sizeof(WCHAR);if(!out)return E_POINTER;out->uType=STRRET_WSTR;out->pOleStr=CoTaskMemAlloc(bytes);if(!out->pOleStr)return E_OUTOFMEMORY;memcpy(out->pOleStr,s,bytes);return S_OK;}
static HRESULT WINAPI folder_setname(IShellFolder*p,HWND h,LPCITEMIDLIST c,LPCWSTR n,DWORD f,LPITEMIDLIST*o){(void)p;(void)h;(void)c;(void)n;(void)f;(void)o;return E_FAIL;}
static const IShellFolderVtbl folder_vtbl={folder_qi,folder_add,folder_rel,folder_parse,folder_enum,folder_bind,folder_fail4,folder_compare,folder_view,folder_attr,folder_ui,folder_name,folder_setname};
static HRESULT WINAPI persist_qi(IPersistFolder2*p,REFIID i,void**o){return folder_qi(&from_persist(p)->folder,i,o);}static ULONG WINAPI persist_add(IPersistFolder2*p){return ++from_persist(p)->refs;}static ULONG WINAPI persist_rel(IPersistFolder2*p){return folder_rel(&from_persist(p)->folder);}static HRESULT WINAPI persist_class(IPersistFolder2*p,CLSID*c){(void)p;if(!c)return E_POINTER;memset(c,0,sizeof(*c));return S_OK;}static HRESULT WINAPI persist_init(IPersistFolder2*p,LPCITEMIDLIST i){SHELL_FOLDER*f=from_persist(p);wcopy(f->path,pidl_text(i));return S_OK;}static HRESULT WINAPI persist_cur(IPersistFolder2*p,LPITEMIDLIST*o){if(!o)return E_POINTER;*o=make_pidl(from_persist(p)->path);return *o?S_OK:E_OUTOFMEMORY;}
static const IPersistFolder2Vtbl persist_vtbl={persist_qi,persist_add,persist_rel,persist_class,persist_init,persist_cur};
static HRESULT folder_create(LPCWSTR path,REFIID iid,void **out){SHELL_FOLDER*f=zalloc(sizeof(*f));HRESULT hr;if(!f)return E_OUTOFMEMORY;f->folder.lpVtbl=&folder_vtbl;f->persist.lpVtbl=&persist_vtbl;f->refs=1;wcopy(f->path,path);hr=folder_qi(&f->folder,iid,out);folder_rel(&f->folder);return hr;}
HRESULT WINAPI SHGetDesktopFolder(IShellFolder **out){return out?folder_create(L"/",&IID_IShellFolder,(void**)out):E_POINTER;}
HRESULT WINAPI SHGetSpecialFolderLocation(HWND h,int id,LPITEMIDLIST*out){(void)h;if(!out)return E_POINTER;*out=make_pidl(id==CSIDL_CONTROLS?L"/DISCOUNT/SYSTEM32":L"/");return *out?S_OK:E_OUTOFMEMORY;}
HRESULT WINAPI SHGetKnownFolderPath(REFGUID id,DWORD flags,HANDLE token,LPWSTR*out){LPCWSTR s=L"/";UINT bytes;(void)flags;(void)token;if(!out)return E_POINTER;if(IsEqualGUID(id,&FOLDERID_Desktop)||IsEqualGUID(id,&FOLDERID_PublicDesktop))s=L"/";bytes=(wlen(s)+1)*sizeof(WCHAR);*out=CoTaskMemAlloc(bytes);if(!*out)return E_OUTOFMEMORY;memcpy(*out,s,bytes);return S_OK;}
HRESULT WINAPI SHBindToParent(LPCITEMIDLIST p,REFIID iid,void**parent,LPCITEMIDLIST*child){if(child)*child=p;return folder_create(L"/",iid,parent);}
HRESULT WINAPI SHGetIDListFromObject(IUnknown*object,LPITEMIDLIST*out){IPersistFolder2*p;HRESULT hr;if(!object||!out)return E_INVALIDARG;hr=IUnknown_QueryInterface(object,&IID_IPersistFolder2,(void**)&p);if(FAILED(hr))return hr;hr=IPersistFolder2_GetCurFolder(p,out);IPersistFolder2_Release(p);return hr;}
HRESULT WINAPI StrRetToStrW(STRRET*s,LPCITEMIDLIST p,LPWSTR*out){LPCWSTR text;UINT bytes;(void)p;if(!s||!out)return E_INVALIDARG;text=s->uType==STRRET_WSTR?s->pOleStr:L"";bytes=(wlen(text)+1)*sizeof(WCHAR);*out=CoTaskMemAlloc(bytes);if(!*out)return E_OUTOFMEMORY;memcpy(*out,text,bytes);if(s->uType==STRRET_WSTR)CoTaskMemFree(s->pOleStr);return S_OK;}
