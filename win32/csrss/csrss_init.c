#include <stdint.h>
#include "csrss.h"
#include "core/ke.h"
#include "serial.h"

static HANDLE csrss_process = INVALID_HANDLE;

int CsrssInitialize(void *boot_info) {
    (void)boot_info;
    if (csrss_process != INVALID_HANDLE) return 1;

    SerialPutString("[CSRSS] Initializing Client/Server Runtime Subsystem\r\n");
    csrss_process = KeCreateProcess("Client/Server Runtime");
    if (csrss_process == INVALID_HANDLE) {
        SerialPutString("[CSRSS] Process creation failed\r\n");
        return 0;
    }

    SerialPutString("[CSRSS] Runtime process initialized\r\n");
    return 1;
}

int CsrssIsInitialized(void) {
    return csrss_process != INVALID_HANDLE;
}
