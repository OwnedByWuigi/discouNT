#include <stdint.h>
#include "core/subsystem.h"
#include "smss.h"
#include "csrss.h"
#include "serial.h"

static void *subsystem_mb_info = 0;

void SubsystemInit(void *mb_info) {
    subsystem_mb_info = mb_info;
    SmssInitialize();
}

void SubsystemLaunchSmss(void) {
#if defined(__loongarch64)
    /* The LA64 bootstrap is native until executable storage is available. */
    SerialPutString("[SMSS] Starting native LA64 CSRSS bootstrap\r\n");
    if (CsrssInitialize(subsystem_mb_info))
        SerialPutString("[SMSS] Handed session to CSRSS\r\n");
#else
    SmssSessionRun(subsystem_mb_info);
#endif
}
