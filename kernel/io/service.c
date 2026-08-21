#include <stdint.h>
#include "io/service.h"
#include "mm/mm.h"
#include "core/util.h"
#include "serial.h"

typedef struct _SERVICE_RUNTIME {
    const SERVICE_DESCRIPTOR *descriptor;
    SERVICE_STATE state;
} SERVICE_RUNTIME;

static SERVICE_RUNTIME *service_runtime;
static uint32_t service_runtime_count;

void ServiceManagerInitialize(void) {
    service_runtime = 0;
    service_runtime_count = 0;
}

static int service_index(const char *name) {
    for (uint32_t i = 0; i < service_runtime_count; i++)
        if (strcmp(service_runtime[i].descriptor->name, name) == 0) return (int)i;
    return -1;
}

static int service_start_one(uint32_t index, SERVICE_START_ROUTINE start, void *context) {
    SERVICE_RUNTIME *runtime = &service_runtime[index];
    const SERVICE_DESCRIPTOR *service = runtime->descriptor;

    if (runtime->state == SERVICE_RUNNING) return 1;
    if (runtime->state == SERVICE_START_PENDING) {
        SerialPutString("[SERV] Dependency cycle: ");
        SerialPutString(service->name);
        SerialPutString("\r\n");
        runtime->state = SERVICE_FAILED;
        return 0;
    }
    if (runtime->state == SERVICE_FAILED) return 0;

    runtime->state = SERVICE_START_PENDING;
    for (uint32_t i = 0; i < service->dependency_count; i++) {
        int dependency = service_index(service->dependencies[i]);
        if (dependency < 0 || !service_start_one((uint32_t)dependency, start, context)) {
            SerialPutString("[SERV] Dependency failed for ");
            SerialPutString(service->name);
            SerialPutString("\r\n");
            runtime->state = SERVICE_FAILED;
            return 0;
        }
    }

    SerialPutString("[SERV] Starting ");
    SerialPutString(service->name);
    SerialPutString(" (group=");
    SerialPutString(service->group ? service->group : "");
    SerialPutString(")\r\n");
    if (!start(service, context)) {
        runtime->state = SERVICE_FAILED;
        return 0;
    }
    runtime->state = SERVICE_RUNNING;
    return 1;
}

void ServiceManagerInitialize(void);

int ServiceManagerStart(const SERVICE_DESCRIPTOR *services, uint32_t count,
                        SERVICE_START_ROUTINE start, void *context) {
    if (!services || !count || !start) return 0;
    service_runtime = (SERVICE_RUNTIME*)kmalloc(count * sizeof(SERVICE_RUNTIME));
    if (!service_runtime) return 0;
    service_runtime_count = count;
    for (uint32_t i = 0; i < count; i++) {
        service_runtime[i].descriptor = &services[i];
        service_runtime[i].state = SERVICE_STOPPED;
    }

    /* Windows starts services by start type and load-order group. Dependencies
       still determine the exact order within each phase. */
    for (SERVICE_START_TYPE type = SERVICE_BOOT_START; type <= SERVICE_AUTO_START; type++)
        for (uint32_t i = 0; i < count; i++)
            if (services[i].start_type == type)
                service_start_one(i, start, context);
    return 1;
}

SERVICE_STATE ServiceManagerGetState(const char *name) {
    int index = service_index(name);
    return index < 0 ? SERVICE_STOPPED : service_runtime[index].state;
}
