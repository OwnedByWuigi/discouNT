#include "../directx_stub.h"

HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version,
                                  REFIID iid, void **object, void *outer) {
    (void)instance; (void)version; (void)iid; (void)outer;
    return dx_not_implemented(object);
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    return dx_dll_main(module, reason, reserved);
}
