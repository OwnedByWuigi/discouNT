#include "../directx_stub.h"

void *WINAPI Direct3DCreate9(UINT sdk_version) {
    (void)sdk_version; return 0;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT sdk_version, void **object) {
    (void)sdk_version; return dx_not_implemented(object);
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    return dx_dll_main(module, reason, reserved);
}
