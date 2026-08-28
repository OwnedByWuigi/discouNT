#include "../directx_stub.h"

HRESULT WINAPI DirectSoundCreate(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundCreate8(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundCaptureCreate(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundEnumerateA(void *callback, void *context) {
    (void)callback; (void)context; return DX_E_NOTIMPL;
}

HRESULT WINAPI DirectSoundEnumerateW(void *callback, void *context) {
    (void)callback; (void)context; return DX_E_NOTIMPL;
}

HRESULT WINAPI DirectSoundCaptureEnumerateW(void *callback, void *context) {
    (void)callback; (void)context; return DX_E_NOTIMPL;
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    return dx_dll_main(module, reason, reserved);
}
