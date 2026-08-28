#include "ddraw.h"
#include "string.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *memory);
extern int FbGetWidth(void);
extern int FbGetHeight(void);

typedef struct { IDirectDraw7 iface; ULONG refs; } DDRAW_OBJECT;
#define DX_E_NOTIMPL ((HRESULT)0x80004001L)

static ULONG WINAPI ddraw_addref(IDirectDraw7 *iface) { return ++((DDRAW_OBJECT *)iface)->refs; }
static ULONG WINAPI ddraw_release(IDirectDraw7 *iface) {
    DDRAW_OBJECT *object = (DDRAW_OBJECT *)iface;
    if (object->refs) object->refs--;
    if (!object->refs) kfree(object);
    return object->refs;
}
static HRESULT WINAPI ddraw_qi(IDirectDraw7 *iface, REFIID iid, void **out) {
    if (!out) return E_POINTER;
    *out = 0;
    if (!iid || IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDirectDraw7)) {
        *out = iface; ddraw_addref(iface); return S_OK;
    }
    return E_NOINTERFACE;
}
static HRESULT WINAPI ddraw_mem(IDirectDraw7 *iface, DDSCAPS2 *caps, DWORD *total, DWORD *free_mem) {
    (void)iface; (void)caps;
    if (total) *total = 16ULL * 1024 * 1024;
    if (free_mem) *free_mem = 16ULL * 1024 * 1024;
    return S_OK;
}
static HRESULT WINAPI ddraw_mode(IDirectDraw7 *iface, DDSURFACEDESC2 *mode) {
    (void)iface;
    if (!mode) return E_POINTER;
    memset(mode, 0, sizeof(*mode));
    mode->dwSize = sizeof(*mode);
    mode->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    mode->dwWidth = (DWORD)(FbGetWidth() > 0 ? FbGetWidth() : 640);
    mode->dwHeight = (DWORD)(FbGetHeight() > 0 ? FbGetHeight() : 480);
    mode->ddpfPixelFormat.dwSize = sizeof(mode->ddpfPixelFormat);
    mode->ddpfPixelFormat.dwRGBBitCount = 32;
    return S_OK;
}
static const IDirectDraw7Vtbl ddraw_vtbl = {
    {(void *)ddraw_qi, (void *)ddraw_addref, (void *)ddraw_release},
    ddraw_mem, ddraw_mode
};

HRESULT WINAPI DirectDrawCreate(GUID *guid, void **object, void *outer) {
    return DirectDrawCreateEx(guid, object, &IID_IDirectDraw7, outer);
}

HRESULT WINAPI DirectDrawCreateEx(GUID *guid, void **object, REFIID iid, IUnknown *outer) {
    DDRAW_OBJECT *result;
    (void)guid;
    if (!object) return E_POINTER;
    *object = 0;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (iid && !IsEqualIID(iid, &IID_IDirectDraw7) && !IsEqualIID(iid, &IID_IUnknown)) return E_NOINTERFACE;
    result = (DDRAW_OBJECT *)kmalloc(sizeof(*result));
    if (!result) return E_OUTOFMEMORY;
    result->iface.lpVtbl = &ddraw_vtbl;
    result->refs = 0;
    return ddraw_qi(&result->iface, iid, object);
}

HRESULT WINAPI DirectDrawEnumerateA(void *callback, void *context) {
    (void)callback; (void)context; return DX_E_NOTIMPL;
}

HRESULT WINAPI DirectDrawEnumerateW(void *callback, void *context) {
    (void)callback; (void)context; return DX_E_NOTIMPL;
}

HRESULT WINAPI DirectDrawEnumerateExA(void *callback, void *context, DWORD flags) {
    (void)callback; (void)context; (void)flags; return DX_E_NOTIMPL;
}

HRESULT WINAPI DirectDrawEnumerateExW(void *callback, void *context, DWORD flags) {
    (void)callback; (void)context; (void)flags; return DX_E_NOTIMPL;
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    (void)module; (void)reason; (void)reserved;
    return 1;
}
