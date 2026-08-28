#ifndef DISCOUNT_DIRECTX_STUB_H
#define DISCOUNT_DIRECTX_STUB_H

#include "../include/win32/windows.h"

#define DX_E_NOTIMPL ((HRESULT)0x80004001UL)

/* These entry points intentionally provide a loadable compatibility surface.
 * Rendering and audio backends can be added behind them without making every
 * DirectX-using program fail during DLL resolution. */
static inline HRESULT dx_not_implemented(void **object) {
    if (object) *object = 0;
    return DX_E_NOTIMPL;
}

static inline int dx_dll_main(void *module, DWORD reason, void *reserved) {
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}

#endif
