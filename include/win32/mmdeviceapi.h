#ifndef DISCOUNT_MMDEVICEAPI_H
#define DISCOUNT_MMDEVICEAPI_H
#include "objbase.h"
typedef UINT UINT32;
typedef struct { GUID fmtid; DWORD pid; } PROPERTYKEY;
typedef VARIANT PROPVARIANT;
typedef struct IMMDevice IMMDevice;
typedef struct IMMDeviceCollection IMMDeviceCollection;
typedef struct IMMDeviceEnumerator IMMDeviceEnumerator;
typedef struct IPropertyStore IPropertyStore;
typedef struct IMMDeviceCollectionVtbl {
    HRESULT (*QueryInterface)(IMMDeviceCollection*,REFIID,void**); ULONG (*AddRef)(IMMDeviceCollection*); ULONG (*Release)(IMMDeviceCollection*);
    HRESULT (*GetCount)(IMMDeviceCollection*,UINT*); HRESULT (*Item)(IMMDeviceCollection*,UINT,IMMDevice**);
} IMMDeviceCollectionVtbl;
struct IMMDeviceCollection { const IMMDeviceCollectionVtbl *lpVtbl; };
typedef struct IMMDeviceVtbl {
    HRESULT (*QueryInterface)(IMMDevice*,REFIID,void**); ULONG (*AddRef)(IMMDevice*); ULONG (*Release)(IMMDevice*);
    HRESULT (*Activate)(); HRESULT (*OpenPropertyStore)(IMMDevice*,DWORD,IPropertyStore**); HRESULT (*GetId)(); HRESULT (*GetState)();
} IMMDeviceVtbl;
struct IMMDevice { const IMMDeviceVtbl *lpVtbl; };
typedef struct IPropertyStoreVtbl {
    HRESULT (*QueryInterface)(IPropertyStore*,REFIID,void**); ULONG (*AddRef)(IPropertyStore*); ULONG (*Release)(IPropertyStore*);
    HRESULT (*GetCount)(); HRESULT (*GetAt)(); HRESULT (*GetValue)(IPropertyStore*,const PROPERTYKEY*,PROPVARIANT*); HRESULT (*SetValue)(); HRESULT (*Commit)();
} IPropertyStoreVtbl;
struct IPropertyStore { const IPropertyStoreVtbl *lpVtbl; };
typedef struct IMMDeviceEnumeratorVtbl {
    HRESULT (*QueryInterface)(IMMDeviceEnumerator*,REFIID,void**); ULONG (*AddRef)(IMMDeviceEnumerator*); ULONG (*Release)(IMMDeviceEnumerator*);
    HRESULT (*EnumAudioEndpoints)(IMMDeviceEnumerator*,DWORD,DWORD,IMMDeviceCollection**);
} IMMDeviceEnumeratorVtbl;
struct IMMDeviceEnumerator { const IMMDeviceEnumeratorVtbl *lpVtbl; };
#define IMMDeviceCollection_GetCount(p,a) ((p)->lpVtbl->GetCount((p),(a)))
#define IMMDeviceCollection_Item(p,a,b) ((p)->lpVtbl->Item((p),(a),(b)))
#define IMMDeviceCollection_Release(p) ((p)->lpVtbl->Release((p)))
#define IMMDevice_OpenPropertyStore(p,a,b) ((p)->lpVtbl->OpenPropertyStore((p),(a),(b)))
#define IMMDevice_Release(p) ((p)->lpVtbl->Release((p)))
#define IPropertyStore_GetValue(p,a,b) ((p)->lpVtbl->GetValue((p),(a),(b)))
#define IPropertyStore_Release(p) ((p)->lpVtbl->Release((p)))
#define IMMDeviceEnumerator_EnumAudioEndpoints(p,a,b,c) ((p)->lpVtbl->EnumAudioEndpoints((p),(a),(b),(c)))
#define IMMDeviceEnumerator_Release(p) ((p)->lpVtbl->Release((p)))
#define eAll 3
#define DEVICE_STATE_ACTIVE 1
#define STGM_READ 0
static const CLSID CLSID_MMDeviceEnumerator={0xbcde0395,0xe52f,0x467c,{0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const IID IID_IMMDeviceEnumerator={0xa95664d2,0x9614,0x4f35,{0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const PROPERTYKEY PKEY_AudioEndpoint_GUID={{0x1da5d803,0xd492,0x4edd,{0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e}},4};
static inline void PropVariantInit(PROPVARIANT *v) { VariantInit(v); }
static inline HRESULT PropVariantClear(PROPVARIANT *v) { return VariantClear(v); }
#endif
