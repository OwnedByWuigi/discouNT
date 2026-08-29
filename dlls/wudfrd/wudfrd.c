#include <stdint.h>
#include <stddef.h>
#include "../../compat/wdf/platform/include/wdf_umdf.h"
#include "../../compat/wdf/platform/include/wdf_transport.h"

#define EXPORT __attribute__((visibility("default")))
#define STATUS_SUCCESS ((WDF_UMDF_NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((WDF_UMDF_NTSTATUS)-1073741811)
#define STATUS_INSUFFICIENT_RESOURCES ((WDF_UMDF_NTSTATUS)-1073741801)
#define STATUS_NOT_SUPPORTED ((WDF_UMDF_NTSTATUS)-1073741637)

#define UMDF_OBJECT_MAGIC 0x5544464Fu
#define UMDF_MAX_OBJECTS 128
#define UMDF_MAX_MEMORY 65536

typedef struct {
    uint32_t magic;
    uint16_t type;
    uint16_t used;
    uint32_t refs;
    void *parent;
    void *context;
    size_t context_size;
} UMDF_OBJECT;

typedef struct {
    UMDF_OBJECT object;
    void *device_add;
    void *unload;
} UMDF_DRIVER;

typedef struct {
    UMDF_OBJECT object;
    UMDF_DRIVER *driver;
    void *default_queue;
    uint32_t endpoint;
} UMDF_DEVICE;

typedef struct {
    UMDF_OBJECT object;
    UMDF_DEVICE *device;
    void *default_callback;
    void *read_callback;
    void *write_callback;
    void *control_callback;
} UMDF_QUEUE;

typedef struct {
    UMDF_OBJECT object;
    void *buffer;
    size_t length;
    WDF_UMDF_NTSTATUS status;
    uint32_t information;
} UMDF_REQUEST;

typedef struct {
    uint32_t Size;
    void *cleanup;
    void *destroy;
    uint32_t execution_level;
    uint32_t synchronization_scope;
    void *parent;
    size_t context_size_override;
    const void *context_type_info;
} UMDF_ATTRIBUTES;

typedef struct {
    uint32_t Size;
    void *device_add;
    void *unload;
    uint32_t flags;
    uint32_t tag;
} UMDF_DRIVER_CONFIG;

typedef struct {
    uint32_t Size;
    uint32_t dispatch_type;
    uint32_t power_managed;
    uint8_t allow_zero;
    uint8_t default_queue;
    void *default_callback;
    void *read_callback;
    void *write_callback;
    void *control_callback;
} UMDF_QUEUE_CONFIG;

static UMDF_OBJECT object_pool[UMDF_MAX_OBJECTS];
static uint8_t memory_pool[UMDF_MAX_MEMORY];
static size_t memory_used;
static UMDF_DEVICE *devices[UMDF_MAX_OBJECTS];
static uint32_t device_count;

extern uint32_t WudfTransportRegisterEndpoint(void);
extern int WudfTransportPoll(uint32_t endpoint, WDF_TRANSPORT_PACKET *packet);
extern int WudfTransportComplete(uint32_t endpoint, uint32_t token, int32_t status,
                                 uint32_t information, const void *data, uint32_t data_length);

WDF_UMDF_GLOBALS *WdfDriverGlobals;

static UMDF_OBJECT *umdf_alloc(uint16_t type, size_t size, UMDF_ATTRIBUTES *attributes)
{
    uint32_t i;
    UMDF_OBJECT *object = NULL;
    (void)size;
    for (i = 0; i < UMDF_MAX_OBJECTS; ++i) {
        if (!object_pool[i].used) { object = &object_pool[i]; break; }
    }
    if (!object) return NULL;
    object->magic = UMDF_OBJECT_MAGIC;
    object->type = type;
    object->used = 1;
    object->refs = 1;
    if (attributes) {
        object->parent = attributes->parent;
        object->context_size = attributes->context_size_override;
        if (object->context_size) {
            if (memory_used + object->context_size > UMDF_MAX_MEMORY) {
                object->used = 0;
                return NULL;
            }
            object->context = &memory_pool[memory_used];
            memory_used += object->context_size;
        }
    }
    return object;
}

static UMDF_OBJECT *umdf_valid(void *handle)
{
    UMDF_OBJECT *object = (UMDF_OBJECT *)handle;
    return object && object->magic == UMDF_OBJECT_MAGIC && object->used ? object : NULL;
}

static void umdf_delete(void *handle)
{
    UMDF_OBJECT *object = umdf_valid(handle);
    if (!object) return;
    object->used = 0;
    object->magic = 0;
}

static WDF_UMDF_NTSTATUS umdf_driver_create(WDF_UMDF_GLOBALS *globals,
                                             void *driver_object, void *registry,
                                             UMDF_ATTRIBUTES *attributes,
                                             UMDF_DRIVER_CONFIG *config,
                                             void **driver_handle)
{
    UMDF_DRIVER *driver;
    (void)driver_object; (void)registry;
    if (!config || !driver_handle) return STATUS_INVALID_PARAMETER;
    driver = (UMDF_DRIVER *)umdf_alloc(1, sizeof(*driver), attributes);
    if (!driver) return STATUS_INSUFFICIENT_RESOURCES;
    driver->device_add = config->device_add;
    driver->unload = config->unload;
    if (!WdfDriverGlobals) WdfDriverGlobals = globals;
    if (globals) {
        globals->Driver = driver;
        globals->DriverFlags = config->flags;
        globals->DriverTag = config->tag;
    }
    *driver_handle = driver;
    return STATUS_SUCCESS;
}

static WDF_UMDF_NTSTATUS umdf_device_create(WDF_UMDF_GLOBALS *globals, void **init,
                                             UMDF_ATTRIBUTES *attributes, void **handle)
{
    UMDF_DEVICE *device;
    (void)init;
    if (!handle || !globals || !umdf_valid(globals->Driver)) return STATUS_INVALID_PARAMETER;
    device = (UMDF_DEVICE *)umdf_alloc(2, sizeof(*device), attributes);
    if (!device || device_count == UMDF_MAX_OBJECTS) return STATUS_INSUFFICIENT_RESOURCES;
    device->driver = (UMDF_DRIVER *)globals->Driver;
    device->endpoint = WudfTransportRegisterEndpoint();
    if (!device->endpoint) { umdf_delete(device); return STATUS_INSUFFICIENT_RESOURCES; }
    devices[device_count++] = device;
    *handle = device;
    return STATUS_SUCCESS;
}

static WDF_UMDF_NTSTATUS umdf_queue_create(WDF_UMDF_GLOBALS *globals, void *device_handle,
                                            UMDF_QUEUE_CONFIG *config, UMDF_ATTRIBUTES *attributes,
                                            void **handle)
{
    UMDF_QUEUE *queue;
    UMDF_DEVICE *device = (UMDF_DEVICE *)umdf_valid(device_handle);
    (void)globals;
    if (!device || !config || !handle) return STATUS_INVALID_PARAMETER;
    queue = (UMDF_QUEUE *)umdf_alloc(3, sizeof(*queue), attributes);
    if (!queue) return STATUS_INSUFFICIENT_RESOURCES;
    queue->device = device;
    queue->default_callback = config->default_callback;
    queue->read_callback = config->read_callback;
    queue->write_callback = config->write_callback;
    queue->control_callback = config->control_callback;
    if (config->default_queue) device->default_queue = queue;
    *handle = queue;
    return STATUS_SUCCESS;
}

static WDF_UMDF_NTSTATUS umdf_request_create(WDF_UMDF_GLOBALS *globals, UMDF_ATTRIBUTES *attributes,
                                              void *target, void **handle)
{
    UMDF_REQUEST *request;
    (void)globals; (void)target;
    if (!handle) return STATUS_INVALID_PARAMETER;
    request = (UMDF_REQUEST *)umdf_alloc(4, sizeof(*request), attributes);
    if (!request) return STATUS_INSUFFICIENT_RESOURCES;
    request->status = STATUS_SUCCESS;
    *handle = request;
    return STATUS_SUCCESS;
}

static void umdf_request_complete(WDF_UMDF_GLOBALS *globals, void *handle, WDF_UMDF_NTSTATUS status)
{
    UMDF_REQUEST *request = (UMDF_REQUEST *)umdf_valid(handle);
    (void)globals;
    if (request) request->status = status;
}

static WDF_UMDF_NTSTATUS umdf_request_status(WDF_UMDF_GLOBALS *globals, void *handle)
{
    UMDF_REQUEST *request = (UMDF_REQUEST *)umdf_valid(handle);
    (void)globals;
    return request ? request->status : STATUS_INVALID_PARAMETER;
}

static WDF_UMDF_FUNC umdf_functions[256];
EXPORT const WDF_UMDF_FUNC *WdfFunctions_02017 = umdf_functions;
static WDF_UMDF_NTSTATUS umdf_unsupported(void) { return STATUS_NOT_SUPPORTED; }

EXPORT WDF_UMDF_NTSTATUS WudfHostDispatch(WDF_UMDF_HANDLE device_handle, uint32_t major,
                                          void *buffer, uint32_t length, uint32_t ioctl_code,
                                          const void *input, uint32_t input_length,
                                          uint32_t *information)
{
    UMDF_DEVICE *device = (UMDF_DEVICE *)umdf_valid(device_handle);
    UMDF_QUEUE *queue;
    UMDF_REQUEST request;
    (void)input; (void)input_length;
    if (!device || !device->default_queue) return STATUS_INVALID_PARAMETER;
    queue = (UMDF_QUEUE *)device->default_queue;
    request.object.magic = UMDF_OBJECT_MAGIC;
    request.object.used = 1;
    request.buffer = buffer;
    request.length = length;
    request.status = STATUS_SUCCESS;
    if (major == 2 && queue->read_callback)
        ((void (*)(void *, void *, size_t))queue->read_callback)(queue, &request, length);
    else if (major == 3 && queue->write_callback)
        ((void (*)(void *, void *, size_t))queue->write_callback)(queue, &request, length);
    else if (major == 4 && queue->control_callback)
        ((void (*)(void *, void *, size_t, size_t, uint32_t))queue->control_callback)(queue, &request, length, input_length, ioctl_code);
    else if (queue->default_callback)
        ((void (*)(void *, void *))queue->default_callback)(queue, &request);
    else request.status = STATUS_NOT_SUPPORTED;
    if (information) *information = request.information;
    return request.status;
}

EXPORT int WudfHostPump(void)
{
    uint32_t i;
    int handled = 0;
    for (i = 0; i < device_count; ++i) {
        WDF_TRANSPORT_PACKET packet;
        UMDF_DEVICE *device = devices[i];
        UMDF_QUEUE *queue;
        UMDF_REQUEST request;
        if (!device || !device->endpoint || !WudfTransportPoll(device->endpoint, &packet)) continue;
        queue = (UMDF_QUEUE *)device->default_queue;
        if (!queue) { WudfTransportComplete(device->endpoint, packet.token, STATUS_NOT_SUPPORTED, 0, 0, 0); continue; }
        request.object.magic = UMDF_OBJECT_MAGIC; request.object.used = 1;
        request.buffer = packet.data; request.length = packet.data_length; request.status = STATUS_SUCCESS;
        if (packet.major_function == 2 && queue->read_callback)
            ((void (*)(void *, void *, size_t))queue->read_callback)(queue, &request, packet.output_length);
        else if (packet.major_function == 3 && queue->write_callback)
            ((void (*)(void *, void *, size_t))queue->write_callback)(queue, &request, packet.input_length);
        else if (packet.major_function == 4 && queue->control_callback)
            ((void (*)(void *, void *, size_t, size_t, uint32_t))queue->control_callback)(queue, &request, packet.output_length, packet.input_length, packet.ioctl_code);
        else if (queue->default_callback)
            ((void (*)(void *, void *))queue->default_callback)(queue, &request);
        else request.status = STATUS_NOT_SUPPORTED;
        WudfTransportComplete(device->endpoint, packet.token, request.status, request.information,
                               packet.data, request.information);
        handled = 1;
    }
    return handled;
}

EXPORT int DllMain(void *module, uint32_t reason, void *reserved)
{
    uint32_t i;
    (void)module; (void)reserved;
    if (reason != 1) return 1;
    for (i = 0; i < 256; ++i) umdf_functions[i] = (WDF_UMDF_FUNC)umdf_unsupported;
    umdf_functions[25] = (WDF_UMDF_FUNC)umdf_device_create;
    umdf_functions[57] = (WDF_UMDF_FUNC)umdf_driver_create;
    umdf_functions[85] = (WDF_UMDF_FUNC)umdf_queue_create;
    umdf_functions[129] = (WDF_UMDF_FUNC)umdf_delete;
    umdf_functions[148] = (WDF_UMDF_FUNC)umdf_request_create;
    umdf_functions[163] = (WDF_UMDF_FUNC)umdf_request_complete;
    return 1;
}
