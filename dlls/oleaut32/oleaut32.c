#include "oaidl.h"
#include "string.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *memory);

SAFEARRAY *WINAPI SafeArrayCreateVector(VARTYPE type,LONG lower,ULONG count){
    SAFEARRAY *a=(SAFEARRAY*)kmalloc(sizeof(*a));ULONG bytes;if(!a)return 0;
    a->cDims=1;a->fFeatures=0;a->cbElements=(type==VT_UI1)?1:sizeof(void*);a->cLocks=0;a->rgsabound[0].cElements=count;a->rgsabound[0].lLbound=lower;
    bytes=count*a->cbElements;a->pvData=kmalloc(bytes);if(!a->pvData){kfree(a);return 0;}memset(a->pvData,0,bytes);return a;
}
HRESULT WINAPI SafeArrayDestroy(SAFEARRAY *a){if(!a)return E_INVALIDARG;kfree(a->pvData);kfree(a);return S_OK;}
void WINAPI VariantInit(VARIANT *v){if(v)memset(v,0,sizeof(*v));}
HRESULT WINAPI VariantClear(VARIANT *v){if(!v)return E_INVALIDARG;if((v->vt&VT_ARRAY)&&v->parray)SafeArrayDestroy(v->parray);else if(v->vt==8&&v->bstrVal)SysFreeString(v->bstrVal);VariantInit(v);return S_OK;}
HRESULT WINAPI VariantCopy(VARIANT *d,const VARIANT *s){if(!d||!s)return E_INVALIDARG;VariantClear(d);*d=*s;if(s->vt==VT_BSTR&&s->bstrVal)d->bstrVal=SysAllocString(s->bstrVal);return S_OK;}
HRESULT WINAPI VariantChangeType(VARIANT *d,VARIANT *s,WORD flags,VARTYPE type){(void)flags;if(!d||!s)return E_INVALIDARG;if(type==VT_BSTR){WCHAR b[32];if(s->vt==VT_BSTR)return VariantCopy(d,s);if(s->vt==VT_UI8) _ui64tow(s->ullVal,b,10);else {b[0]=L'0';b[1]=0;}VariantClear(d);d->vt=VT_BSTR;d->bstrVal=SysAllocString(b);return d->bstrVal?S_OK:E_OUTOFMEMORY;}return E_NOTIMPL;}
HRESULT WINAPI SafeArrayAccessData(SAFEARRAY *a,void **data){if(!a||!data)return E_INVALIDARG;*data=a->pvData;return S_OK;}
HRESULT WINAPI SafeArrayUnaccessData(SAFEARRAY *a){return a?S_OK:E_INVALIDARG;}
BSTR WINAPI SysAllocString(const WCHAR *s){SIZE_T n=0;WCHAR *d;if(!s)return 0;while(s[n])n++;d=(WCHAR*)kmalloc((uint32_t)((n+1)*sizeof(WCHAR)));if(d)memcpy(d,s,(uint32_t)((n+1)*sizeof(WCHAR)));return d;}
void WINAPI SysFreeString(BSTR s){kfree(s);}
HRESULT WINAPI LoadRegTypeLib(REFGUID libid,WORD major,WORD minor,LCID locale,ITypeLib **library){(void)libid;(void)major;(void)minor;(void)locale;if(!library)return E_POINTER;*library=0;return E_NOTIMPL;}
int WINAPI DllMain(void *module,DWORD reason,void *reserved){(void)module;(void)reason;(void)reserved;return 1;}
