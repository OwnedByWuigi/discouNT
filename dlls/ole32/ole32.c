#include "objbase.h"
#include "string.h"
#include "wbemcli.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *memory);

#define COM_CLASSES 32
typedef struct { CLSID clsid; IClassFactory *factory; DWORD cookie; } COM_CLASS;
static COM_CLASS classes[COM_CLASSES];
static DWORD next_cookie=1;
static const CLSID ole32_dxdiag_provider =
    {0xA65B8071, 0x3BFE, 0x4213, {0x9A, 0x5B, 0x49, 0x1D, 0xA4, 0x46, 0x1C, 0xA7}};

int WINAPI StringFromGUID2(REFGUID g, LPWSTR out, int count)
{
    static const WCHAR hex[] = L"0123456789abcdef";
    WCHAR *p = out; DWORD v; int i;
    if (!g || !out || count < 39) return 0;
    v=g->Data1; for(i=7;i>=0;i--){p[i]=hex[v&15];v>>=4;} p+=8;*p++=L'-';
    v=g->Data2; for(i=3;i>=0;i--){p[i]=hex[v&15];v>>=4;} p+=4;*p++=L'-';
    v=g->Data3; for(i=3;i>=0;i--){p[i]=hex[v&15];v>>=4;} p+=4;*p++=L'-';
    for(i=0;i<2;i++){*p++=hex[g->Data4[i]>>4];*p++=hex[g->Data4[i]&15];} *p++=L'-';
    for(;i<8;i++){*p++=hex[g->Data4[i]>>4];*p++=hex[g->Data4[i]&15];} *p=0; return 39;
}
HRESULT WINAPI CLSIDFromString(LPCWSTR s, CLSID *g) { (void)s; if(!g)return E_POINTER; memset(g,0,sizeof(*g)); return E_FAIL; }

#if 0 /* DxDiag is implemented by dxdiagn.dll, not ole32. */
typedef struct {
    IDxDiagProvider iface;
    ULONG refs;
} DXDIAG_PROVIDER;

typedef struct {
    IDxDiagContainer iface;
    ULONG refs;
} DXDIAG_CONTAINER;

static DXDIAG_PROVIDER dxdiag_provider;
static DXDIAG_CONTAINER dxdiag_container;

static BSTR dxdiag_alloc_string(const WCHAR *text)
{
    SIZE_T length = 0;
    uint16_t *copy;

    while (text && text[length]) length++;
    copy = (uint16_t *)kmalloc((uint32_t)((length + 1) * sizeof(uint16_t)));
    if (!copy) return 0;
    while (length) {
        length--;
        copy[length] = (uint16_t)text[length];
    }
    copy[length] = 0;
    return (BSTR)copy;
}

static int dxdiag_wstrcmp(const uint16_t *a, const WCHAR *b)
{
    while (*a && *b && *a == (uint16_t)*b) { a++; b++; }
    return (int)*a - (int)*b;
}

static int dxdiag_name_equals(const uint16_t *name, const WCHAR *expected)
{
    return !dxdiag_wstrcmp(name, expected);
}

static HRESULT WINAPI dxdiag_provider_qi(IDxDiagProvider *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = 0;
    if (!iid || IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDxDiagProvider)) {
        *out = iface;
        iface->lpVtbl->AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI dxdiag_provider_addref(IDxDiagProvider *iface)
{
    return ++((DXDIAG_PROVIDER *)iface)->refs;
}

static ULONG WINAPI dxdiag_provider_release(IDxDiagProvider *iface)
{
    DXDIAG_PROVIDER *provider = (DXDIAG_PROVIDER *)iface;
    if (provider->refs) provider->refs--;
    return provider->refs;
}

static HRESULT WINAPI dxdiag_provider_initialize(IDxDiagProvider *iface, DXDIAG_INIT_PARAMS *params)
{
    (void)iface; (void)params;
    return S_OK;
}

static HRESULT WINAPI dxdiag_provider_root(IDxDiagProvider *iface, IDxDiagContainer **out)
{
    (void)iface;
    if (!out) return E_POINTER;
    *out = &dxdiag_container.iface;
    dxdiag_container.refs++;
    return S_OK;
}

static const IDxDiagProviderVtbl dxdiag_provider_vtbl = {
    dxdiag_provider_qi, dxdiag_provider_addref, dxdiag_provider_release,
    dxdiag_provider_initialize, dxdiag_provider_root
};

static HRESULT WINAPI dxdiag_container_qi(IDxDiagContainer *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = 0;
    if (!iid || IsEqualIID(iid, &IID_IUnknown)) {
        *out = iface;
        iface->lpVtbl->AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI dxdiag_container_addref(IDxDiagContainer *iface)
{
    return ++((DXDIAG_CONTAINER *)iface)->refs;
}

static ULONG WINAPI dxdiag_container_release(IDxDiagContainer *iface)
{
    DXDIAG_CONTAINER *container = (DXDIAG_CONTAINER *)iface;
    if (container->refs) container->refs--;
    return container->refs;
}

static HRESULT WINAPI dxdiag_container_child(IDxDiagContainer *iface, LPCWSTR name,
                                             IDxDiagContainer **out)
{
    (void)iface; (void)name;
    if (!out) return E_POINTER;
    *out = &dxdiag_container.iface;
    dxdiag_container.refs++;
    return S_OK;
}

static HRESULT WINAPI dxdiag_container_prop(IDxDiagContainer *iface, LPCWSTR name, VARIANT *out)
{
    const WCHAR *value = L"Unknown";
    const uint16_t *utf16_name = (const uint16_t *)name;
    (void)iface;
    if (!name || !out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    if (dxdiag_name_equals(utf16_name, L"szTimeEnglish") || dxdiag_name_equals(utf16_name, L"szTimeLocalized")) value = L"Unavailable";
    else if (dxdiag_name_equals(utf16_name, L"szMachineNameEnglish")) value = L"discouNT";
    else if (dxdiag_name_equals(utf16_name, L"szOSExLongEnglish") || dxdiag_name_equals(utf16_name, L"szOSExLocalized")) value = L"discouNT";
    else if (dxdiag_name_equals(utf16_name, L"szLanguagesEnglish") || dxdiag_name_equals(utf16_name, L"szLanguagesLocalized")) value = L"English";
    else if (dxdiag_name_equals(utf16_name, L"szSystemManufacturerEnglish")) value = L"discouNT";
    else if (dxdiag_name_equals(utf16_name, L"szWindowsDir")) value = L"\\WINDOWS";
    else if (dxdiag_name_equals(utf16_name, L"szDirectXVersionLongEnglish")) value = L"DirectX (compatibility layer)";
    else if (dxdiag_name_equals(utf16_name, L"szDxDiagVersion")) value = L"Wine DxDiag";
    out->vt = VT_BSTR;
    out->bstrVal = dxdiag_alloc_string(value);
    return out->bstrVal ? S_OK : E_OUTOFMEMORY;
}

static const IDxDiagContainerVtbl dxdiag_container_vtbl = {
    dxdiag_container_qi, dxdiag_container_addref, dxdiag_container_release,
    0, 0, dxdiag_container_child, 0, 0, dxdiag_container_prop
};

static void dxdiag_init_objects(void)
{
    if (!dxdiag_provider.iface.lpVtbl) {
        dxdiag_provider.iface.lpVtbl = &dxdiag_provider_vtbl;
        dxdiag_provider.refs = 1;
        dxdiag_container.iface.lpVtbl = &dxdiag_container_vtbl;
        dxdiag_container.refs = 1;
    }
}
#endif

const IID IID_IUnknown={0x00000000,0,0,{0xc0,0,0,0,0,0,0,0x46}};
/* Common shell/URL-completion identifiers used by the unmodified Regedit UI. */
const IID IID_IEnumString={0x00000101,0,0,{0xc0,0,0,0,0,0,0,0x46}};
const IID IID_IAutoComplete={0x00bb2762,0x6a77,0x11d0,{0xa5,0x35,0x00,0xc0,0x4f,0xd7,0xd0,0x62}};
const CLSID CLSID_AutoComplete={0x00bb2763,0x6a77,0x11d0,{0xa5,0x35,0x00,0xc0,0x4f,0xd7,0xd0,0x62}};
const CLSID CLSID_DsObjectPicker={0x17d6ccd8,0x3b7b,0x11d2,{0xb9,0xe0,0x00,0xc0,0x4f,0xd9,0x2e,0x1b}};
const IID IID_IDsObjectPicker={0x7c44,0x4a31,0x11d2,{0xb9,0xe0,0x00,0xc0,0x4f,0xd9,0x2e,0x1b}};
const IID IID_IClassFactory={0x00000001,0,0,{0xc0,0,0,0,0,0,0,0x46}};

HRESULT WINAPI CoInitialize(LPVOID reserved){(void)reserved;return S_OK;}
HRESULT WINAPI CoInitializeEx(LPVOID reserved,DWORD flags){(void)flags;return CoInitialize(reserved);}
void WINAPI CoUninitialize(void){}
LPVOID WINAPI CoTaskMemAlloc(SIZE_T size){return kmalloc((uint32_t)size);}
LPVOID WINAPI CoTaskMemRealloc(LPVOID memory,SIZE_T size){
    LPVOID copy;if(!memory)return kmalloc((uint32_t)size);copy=kmalloc((uint32_t)size);if(copy&&size)memcpy(copy,memory,(uint32_t)size);kfree(memory);return copy;
}
void WINAPI CoTaskMemFree(LPVOID memory){kfree(memory);}
HRESULT WINAPI CoRegisterClassObject(REFCLSID clsid,IUnknown *factory,DWORD context,DWORD flags,DWORD *cookie){
    (void)context;(void)flags;if(!clsid||!factory||!cookie)return E_INVALIDARG;
    for(int i=0;i<COM_CLASSES;i++)if(!classes[i].factory){classes[i].clsid=*clsid;classes[i].factory=(IClassFactory*)factory;IUnknown_AddRef(factory);classes[i].cookie=next_cookie++;*cookie=classes[i].cookie;return S_OK;}return E_OUTOFMEMORY;
}
HRESULT WINAPI CoRevokeClassObject(DWORD cookie){for(int i=0;i<COM_CLASSES;i++)if(classes[i].factory&&classes[i].cookie==cookie){IClassFactory_Release(classes[i].factory);classes[i].factory=0;return S_OK;}return E_INVALIDARG;}
HRESULT WINAPI CoCreateInstance(REFCLSID clsid,IUnknown *outer,DWORD context,REFIID iid,void **object){
    static const CLSID explorer_browser={0x71f96385,0xddd6,0x48d3,{0xa0,0xc1,0xae,0x06,0xe8,0xb0,0x55,0xfb}};
    static const CLSID shell_windows={0x9ba05972,0xf6a8,0x11cf,{0xa4,0x42,0x00,0xa0,0xc9,0x0a,0x8f,0x39}};
    typedef HRESULT (WINAPI *shell_create_fn)(REFIID,void**);
    HMODULE shell; shell_create_fn create;
    (void)context;if(!object)return E_POINTER;*object=0;if(outer)return CLASS_E_NOAGGREGATION;
    if (clsid && IsEqualCLSID(clsid, &CLSID_WbemLocator)) {
        /* The userland DLLs use mixed WCHAR ABIs (the apps use UTF-16
         * wchar_t while the native DLLs do not).  Use the ANSI loader here
         * so the module name cannot be misread as a four-byte wide string. */
        HMODULE wbem = LoadLibraryA("WBEMPROX.DLL");
        typedef HRESULT (WINAPI *create_fn)(REFIID, void **);
        create_fn create = wbem ? (create_fn)GetProcAddress(wbem, "WbemCreateInstance") : 0;
        return create ? create(iid, object) : REGDB_E_CLASSNOTREG;
    }
    if (clsid && IsEqualCLSID(clsid, &ole32_dxdiag_provider)) {
        HMODULE dxdiagn = LoadLibraryA("DXDIAGN.DLL");
        typedef HRESULT (WINAPI *get_class_object_fn)(REFCLSID, REFIID, void **);
        get_class_object_fn get_class_object;
        if (!dxdiagn) return REGDB_E_CLASSNOTREG;
        get_class_object = (get_class_object_fn)GetProcAddress(dxdiagn, "DllGetClassObject");
        if (!get_class_object) return REGDB_E_CLASSNOTREG;
        {
            IClassFactory *factory = 0;
            HRESULT hr = get_class_object(clsid, &IID_IClassFactory, (void **)&factory);
            if (FAILED(hr)) return hr;
            hr = IClassFactory_CreateInstance(factory, 0, iid, object);
            IClassFactory_Release(factory);
            return hr;
        }
    }
    for(int i=0;i<COM_CLASSES;i++)if(classes[i].factory&&IsEqualCLSID(clsid,&classes[i].clsid))return IClassFactory_CreateInstance(classes[i].factory,0,iid,object);
    if(clsid&&IsEqualCLSID(clsid,&explorer_browser)){
        shell=LoadLibraryW(L"SHELL32.DLL");
        if(shell&&(create=(shell_create_fn)GetProcAddress(shell,"ShellCreateExplorerBrowser")))return create(iid,object);
    }
    if(clsid&&IsEqualCLSID(clsid,&shell_windows)){
        shell=LoadLibraryW(L"SHELL32.DLL");
        if(shell&&(create=(shell_create_fn)GetProcAddress(shell,"ShellCreateShellWindows")))return create(iid,object);
    }
    return REGDB_E_CLASSNOTREG;
}
int WINAPI DllMain(void *module,DWORD reason,void *reserved){(void)module;(void)reason;(void)reserved;return 1;}
