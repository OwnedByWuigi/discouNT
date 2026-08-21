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
    SmssSessionRun(subsystem_mb_info);
}
