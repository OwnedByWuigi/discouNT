#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>

typedef enum _SERVICE_TYPE {
    SERVICE_KERNEL_DRIVER = 1,
    SERVICE_FILE_SYSTEM_DRIVER = 2,
    SERVICE_WIN32_SUBSYSTEM = 3
} SERVICE_TYPE;

typedef enum _SERVICE_START_TYPE {
    SERVICE_BOOT_START = 0,
    SERVICE_SYSTEM_START = 1,
    SERVICE_AUTO_START = 2,
    SERVICE_DEMAND_START = 3
} SERVICE_START_TYPE;

typedef enum _SERVICE_STATE {
    SERVICE_STOPPED = 0,
    SERVICE_START_PENDING = 1,
    SERVICE_RUNNING = 2,
    SERVICE_FAILED = 3
} SERVICE_STATE;

typedef struct _SERVICE_DESCRIPTOR {
    const char *name;
    const char *image_path;
    SERVICE_TYPE type;
    SERVICE_START_TYPE start_type;
    const char *group;
    const char * const *dependencies;
    uint32_t dependency_count;
} SERVICE_DESCRIPTOR;

typedef int (*SERVICE_START_ROUTINE)(const SERVICE_DESCRIPTOR *service, void *context);

void ServiceManagerInitialize(void);
int ServiceManagerStart(const SERVICE_DESCRIPTOR *services, uint32_t count,
                        SERVICE_START_ROUTINE start, void *context);
SERVICE_STATE ServiceManagerGetState(const char *name);

#endif
