#include <stdint.h>
#include "core/subsystem.h"
#include "smss.h"

static void *subsystem_mb_info = 0;

void SubsystemInit(void *mb_info) {
    subsystem_mb_info = mb_info;
    SmssInitialize();
}

void SubsystemLaunchSmss(void) {
#if defined(__loongarch64)
    /* CSRSS image loading waits for LA64 storage and executable support. */
    (void)subsystem_mb_info;
#else
    SmssSessionRun(subsystem_mb_info);
#endif
}
