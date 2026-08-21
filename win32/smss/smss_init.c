#include <stdint.h>
#include "smss.h"
#include "core/ke.h"
#include "serial.h"

static HANDLE smss_process = INVALID_HANDLE;

int SmssInitialize(void) {
    if (smss_process != INVALID_HANDLE) return 1;

    SerialPutString("[SMSS] Initializing Session Manager\r\n");
    smss_process = KeCreateProcess("Session Manager");
    if (smss_process == INVALID_HANDLE) {
        SerialPutString("[SMSS] Failed to create Session Manager process\r\n");
        return 0;
    }

    SerialPutString("[SMSS] Session Manager initialized\r\n");
    return 1;
}

int SmssIsInitialized(void) {
    return smss_process != INVALID_HANDLE;
}
