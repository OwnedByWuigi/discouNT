#include "../directx_stub.h"

void *WINAPI Direct3DCreate8(UINT sdk_version) {
    (void)sdk_version; return 0;
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    return dx_dll_main(module, reason, reserved);
}
