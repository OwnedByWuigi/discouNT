#include "wbemcli.h"
#include "oaidl.h"
#include "string.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *p);
extern uint32_t KeGetProcessorCount(void);

typedef struct { IWbemLocator iface; ULONG refs; } LOCATOR;
typedef struct { IWbemServices iface; ULONG refs; } SERVICES;
typedef struct { IEnumWbemClassObject iface; ULONG refs; } ENUM;
typedef struct { IWbemClassObject iface; ULONG refs; } OBJECT;
static const IWbemServicesVtbl sv;
static const IEnumWbemClassObjectVtbl ev;
static const IWbemClassObjectVtbl ov;

static ULONG release(void *p) { ULONG *refs=(ULONG *)((uint8_t *)p+sizeof(void *)); if(*refs)*refs-=1; return *refs; }
static ULONG addref(void *p) { return ++*(ULONG *)((uint8_t *)p+sizeof(void *)); }
static HRESULT qi(void *p, REFIID iid, void **out) { if(!out)return E_POINTER; *out=0; if(!iid || IsEqualIID(iid,&IID_IUnknown)){*out=p;addref(p);return S_OK;} return E_NOINTERFACE; }

static HRESULT locator_connect(IWbemLocator *p,BSTR a,LPCWSTR b,LPCWSTR c,LPCWSTR d,LONG e,LPCWSTR f,IUnknown *g,IWbemServices **out) { static SERVICES s; (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; if(!out)return E_POINTER; s.iface.lpVtbl=&sv; s.refs=1; *out=&s.iface; return S_OK; }
static HRESULT services_enum(IWbemServices *p,BSTR a,LONG b,IUnknown *c,IEnumWbemClassObject **out) { static ENUM e; (void)p;(void)a;(void)b;(void)c;if(!out)return E_POINTER;e.iface.lpVtbl=&ev;e.refs=1;*out=&e.iface;return S_OK; }
static HRESULT enum_next(IEnumWbemClassObject *p,LONG timeout,ULONG count,IWbemClassObject **out,ULONG *fetched) { static OBJECT o; (void)p;(void)timeout;if(!out||!count)return E_POINTER;o.iface.lpVtbl=&ov;o.refs=1;*out=&o.iface;if(fetched)*fetched=1;return S_OK; }
static HRESULT object_get(IWbemClassObject *p,LPCWSTR name,LONG flags,VARIANT *out,LONG *type,LONG *flavor) { (void)p;(void)flags;(void)type;(void)flavor;if(!name||!out)return E_POINTER;VariantInit(out);if(name[0]=='N'&&name[1]=='a'){out->vt=VT_BSTR;out->bstrVal=SysAllocString(L"discouNT CPU");}else {out->vt=VT_I4;out->lVal=(name[0]=='M')?2400:(LONG)KeGetProcessorCount();}return out->bstrVal||out->vt==VT_I4?S_OK:E_OUTOFMEMORY; }
static HRESULT null_qi(void *p,REFIID i,void **o){return qi(p,i,o);}
static const IWbemLocatorVtbl lv={{(void *)null_qi,(void *)addref,(void *)release},locator_connect};
static const IWbemServicesVtbl sv={{(void *)null_qi,(void *)addref,(void *)release},services_enum};
static const IEnumWbemClassObjectVtbl ev={{(void *)null_qi,(void *)addref,(void *)release},enum_next};
static const IWbemClassObjectVtbl ov={{(void *)null_qi,(void *)addref,(void *)release},object_get};

HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void **out) { (void)clsid;(void)iid;(void)out; return CLASS_E_CLASSNOTAVAILABLE; }
HRESULT WINAPI WbemCreateInstance(REFIID iid,void **out) { static LOCATOR l; if(!out)return E_POINTER;l.iface.lpVtbl=&lv;l.refs=1;if(iid&&IsEqualIID(iid,&IID_IWbemLocator)){*out=&l.iface;return S_OK;}return qi(&l.iface,iid,out); }
int WINAPI DllMain(void *m,DWORD r,void *x){(void)m;(void)r;(void)x;return 1;}
