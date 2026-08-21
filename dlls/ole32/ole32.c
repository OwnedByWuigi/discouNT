#include "objbase.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *memory);

#define COM_CLASSES 32
typedef struct { CLSID clsid; IClassFactory *factory; DWORD cookie; } COM_CLASS;
static COM_CLASS classes[COM_CLASSES];
static DWORD next_cookie=1;

const IID IID_IUnknown={0x00000000,0,0,{0xc0,0,0,0,0,0,0,0x46}};
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
