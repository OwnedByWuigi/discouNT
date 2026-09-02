/*
 * Minimal ACLUI platform DLL.
 *
 * Regedit loads ACLUI dynamically when the permissions dialog is requested.
 * Keep this as a normal system DLL so the load and symbol contract works;
 * the full property-sheet editor can be added independently of Regedit.
 */
#include <stdint.h>
#include "windows.h"

__attribute__((stdcall)) int DllMain(void *module, uint32_t reason, void *reserved) {
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}

/* BOOL EditSecurity(HWND, LPSECURITYINFO).  The opaque second parameter is
 * intentionally not re-declared here; ACLUI consumes an interface supplied
 * by the caller. */
__attribute__((stdcall)) int EditSecurity(void *owner, void *security_info) {
    (void)owner;
    (void)security_info;
    return 0;
}
