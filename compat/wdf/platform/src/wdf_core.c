#include <wdf_core.h>

#include <stddef.h>
#include <stdint.h>

extern void *kmalloc(size_t size);
extern void kfree(void *ptr);
extern void *IoCreateDriver(const char *name, void *image, void *context);
extern void *IoCreateDevice(void *driver, const char *name, uint32_t extension_size);
extern void IoDeleteDriver(void *driver);
extern void IoDeleteDevice(void *device);
extern void IoSetPlatformDispatch(void *driver, uint32_t major_function,
                                  int (*dispatch)(void *device, void *request));

#define WDF_OBJECT_MAGIC 0x5744464Fu
#define WDF_FUNCTION_COUNT 512u

enum wdf_object_type {
    WdfObjectDriver = 1,
    WdfObjectDevice,
    WdfObjectQueue,
    WdfObjectRequest,
    WdfObjectMemory,
};

typedef struct wdf_object wdf_object;
typedef struct wdf_driver wdf_driver;
typedef struct wdf_device wdf_device;
typedef struct wdf_queue wdf_queue;
typedef void (*wdf_cleanup_fn)(wdf_object *object);

struct wdf_object {
    uint32_t magic;
    uint16_t type;
    uint16_t flags;
    volatile uint32_t references;
    wdf_object *parent;
    wdf_object *first_child;
    wdf_object *next_sibling;
    void *context;
    size_t context_size;
    wdf_cleanup_fn cleanup;
    wdf_cleanup_fn destroy;
};

struct wdf_driver {
    wdf_object object;
    PDRIVER_OBJECT wdm_driver;
    PUNICODE_STRING registry_path;
    void *device_add;
    void *driver_unload;
    void *io_driver;
    BOOLEAN owns_wdm_driver;
    WDF_DRIVER_GLOBALS globals;
};

struct wdf_device {
    wdf_object object;
    PDEVICE_OBJECT wdm_device;
    wdf_driver *driver;
    void *io_device;
    wdf_queue *default_queue;
    wdf_device *next_global;
};

struct wdf_queue {
    wdf_object object;
    wdf_device *device;
    void *io_config;
    void *evt_default;
    void *evt_read;
    void *evt_write;
    void *evt_device_control;
};

typedef struct {
    wdf_object object;
    void *io_target;
    NTSTATUS status;
    void *io_request;
    PIRP wdm_irp;
} wdf_request;

typedef struct {
    uint32_t major_function;
    struct { int status; uint32_t information; } io_status;
    void *buffer;
    uint32_t length;
    union {
        struct { uint64_t offset; } read_write;
        struct { uint32_t code; const void *input_buffer; uint32_t input_length; } device_control;
    } parameters;
} wdf_io_request;

typedef struct {
    ULONG Size;
    ULONG DispatchType;
    ULONG PowerManaged;
    BOOLEAN AllowZeroLengthRequests;
    BOOLEAN DefaultQueue;
    void *EvtIoDefault;
    void *EvtIoRead;
    void *EvtIoWrite;
    void *EvtIoDeviceControl;
    void *EvtIoInternalDeviceControl;
} wdf_io_queue_config;

typedef struct {
    wdf_object object;
    void *buffer;
    size_t size;
    BOOLEAN owns_buffer;
} wdf_memory;

typedef struct {
    ULONG Size;
    void *EvtCleanupCallback;
    void *EvtDestroyCallback;
    ULONG ExecutionLevel;
    ULONG SynchronizationScope;
    WDFCORE_HANDLE ParentObject;
    size_t ContextSizeOverride;
    const void *ContextTypeInfo;
} wdf_object_attributes;

typedef struct {
    ULONG Size;
    void *EvtDriverDeviceAdd;
    void *EvtDriverUnload;
    ULONG DriverInitFlags;
    ULONG DriverPoolTag;
} wdf_driver_config;

static wdf_device *wdf_devices;
static uint32_t wdf_device_number;
static int wdf_io_dispatch(void *io_device, void *io_request);
static NTSTATUS NTAPI wdf_wdm_dispatch(PDEVICE_OBJECT device_object, PIRP irp);

static void wdf_link_child(wdf_object *parent, wdf_object *child)
{
    if (parent == NULL) {
        return;
    }
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
    __atomic_add_fetch(&parent->references, 1, __ATOMIC_RELAXED);
}

static void wdf_unlink_child(wdf_object *child)
{
    wdf_object **cursor;
    wdf_object *parent = child->parent;

    if (parent == NULL) {
        return;
    }
    cursor = &parent->first_child;
    while (*cursor != NULL) {
        if (*cursor == child) {
            *cursor = child->next_sibling;
            break;
        }
        cursor = &(*cursor)->next_sibling;
    }
    child->parent = NULL;
    child->next_sibling = NULL;
    __atomic_sub_fetch(&parent->references, 1, __ATOMIC_RELAXED);
}

static wdf_object *wdf_new_object(enum wdf_object_type type,
                                  size_t size,
                                  wdf_object_attributes *attributes)
{
    wdf_object *object = (wdf_object *)kmalloc(size);
    if (object == NULL) {
        return NULL;
    }
    RtlZeroMemory(object, size);
    object->magic = WDF_OBJECT_MAGIC;
    object->type = (uint16_t)type;
    object->references = 1;
    if (attributes != NULL && attributes->ContextSizeOverride != 0) {
        object->context_size = attributes->ContextSizeOverride;
        object->context = kmalloc(object->context_size);
        if (object->context == NULL) {
            kfree(object);
            return NULL;
        }
        RtlZeroMemory(object->context, object->context_size);
    }
    if (attributes != NULL) {
        object->cleanup = (wdf_cleanup_fn)attributes->EvtCleanupCallback;
        object->destroy = (wdf_cleanup_fn)attributes->EvtDestroyCallback;
        wdf_link_child((wdf_object *)attributes->ParentObject, object);
    }
    return object;
}

static wdf_object *wdf_from_handle(WDFCORE_HANDLE handle)
{
    wdf_object *object = (wdf_object *)handle;
    if (object == NULL || object->magic != WDF_OBJECT_MAGIC) {
        return NULL;
    }
    return object;
}

static void wdf_destroy_object(wdf_object *object)
{
    wdf_object *child;
    if (object == NULL) {
        return;
    }
    while ((child = object->first_child) != NULL) {
        wdf_destroy_object(child);
    }
    if (object->cleanup != NULL) {
        object->cleanup(object);
    }
    if (object->destroy != NULL) {
        object->destroy(object);
    }
    if (object->type == WdfObjectMemory) {
        wdf_memory *memory = (wdf_memory *)object;
        if (memory->owns_buffer && memory->buffer != NULL) {
            kfree(memory->buffer);
        }
    }
    if (object->type == WdfObjectDevice) {
        wdf_device *device = (wdf_device *)object;
        wdf_device **link = &wdf_devices;
        while (*link != NULL && *link != device) link = &(*link)->next_global;
        if (*link == device) *link = device->next_global;
        if (device->io_device != NULL) IoDeleteDevice(device->io_device);
        if (device->driver != NULL && device->driver->wdm_driver != NULL) {
            PDEVICE_OBJECT *wdm_link = &device->driver->wdm_driver->DeviceObject;
            while (*wdm_link != NULL && *wdm_link != device->wdm_device) {
                wdm_link = &(*wdm_link)->NextDevice;
            }
            if (*wdm_link == device->wdm_device) *wdm_link = device->wdm_device->NextDevice;
        }
        if (device->wdm_device != NULL) kfree(device->wdm_device);
    } else if (object->type == WdfObjectDriver) {
        wdf_driver *driver = (wdf_driver *)object;
        if (driver->io_driver != NULL) IoDeleteDriver(driver->io_driver);
        if (driver->owns_wdm_driver && driver->wdm_driver != NULL) kfree(driver->wdm_driver);
    }
    wdf_unlink_child(object);
    if (object->context != NULL) {
        kfree(object->context);
    }
    object->magic = 0;
    kfree(object);
}

static void wdf_reference(WDFCORE_HANDLE handle)
{
    wdf_object *object = wdf_from_handle(handle);
    if (object != NULL) {
        __atomic_add_fetch(&object->references, 1, __ATOMIC_RELAXED);
    }
}

static void wdf_dereference(WDFCORE_HANDLE handle)
{
    wdf_object *object = wdf_from_handle(handle);
    if (object != NULL && __atomic_sub_fetch(&object->references, 1, __ATOMIC_ACQ_REL) == 0) {
        wdf_destroy_object(object);
    }
}

static NTSTATUS NTAPI wdf_driver_create(PWDF_DRIVER_GLOBALS globals,
                                        PDRIVER_OBJECT driver_object,
                                        const UNICODE_STRING *registry_path,
                                        wdf_object_attributes *attributes,
                                        wdf_driver_config *config,
                                        WDFCORE_HANDLE *driver_handle)
{
    wdf_driver *driver;
    if (config == NULL || driver_handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    driver = (wdf_driver *)wdf_new_object(WdfObjectDriver, sizeof(*driver), attributes);
    if (driver == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    driver->wdm_driver = driver_object;
    if (driver->wdm_driver == NULL) {
        driver->wdm_driver = (PDRIVER_OBJECT)kmalloc(sizeof(DRIVER_OBJECT));
        if (driver->wdm_driver == NULL) {
            wdf_destroy_object(&driver->object);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        RtlZeroMemory(driver->wdm_driver, sizeof(DRIVER_OBJECT));
        driver->wdm_driver->Size = (SHORT)sizeof(DRIVER_OBJECT);
        driver->owns_wdm_driver = TRUE;
    }
    {
        uint32_t major;
        for (major = 0; major < 28; ++major) {
            driver->wdm_driver->MajorFunction[major] = wdf_wdm_dispatch;
        }
    }
    driver->registry_path = (PUNICODE_STRING)registry_path;
    driver->device_add = config->EvtDriverDeviceAdd;
    driver->driver_unload = config->EvtDriverUnload;
    driver->io_driver = IoCreateDriver("WdfDriver", driver_object, driver);
    if (driver->io_driver == NULL) {
        wdf_destroy_object(&driver->object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    {
        uint32_t major;
        for (major = 0; major < 5; ++major) {
            IoSetPlatformDispatch(driver->io_driver, major, wdf_io_dispatch);
        }
    }
    driver->globals.Driver = driver;
    driver->globals.DriverFlags = config->DriverInitFlags;
    driver->globals.DriverTag = config->DriverPoolTag;
    if (globals != NULL) {
        *globals = driver->globals;
    }
    *driver_handle = driver;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_device_create(PWDF_DRIVER_GLOBALS globals,
                                        void **device_init,
                                        wdf_object_attributes *attributes,
                                        WDFCORE_HANDLE *device_handle)
{
    wdf_driver *driver;
    wdf_device *device;
    char name[32];
    (void)device_init;
    if (device_handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    driver = globals != NULL ? (wdf_driver *)wdf_from_handle(globals->Driver) : NULL;
    device = (wdf_device *)wdf_new_object(WdfObjectDevice, sizeof(*device), attributes);
    if (device == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    device->driver = driver;
    device->wdm_device = (PDEVICE_OBJECT)kmalloc(sizeof(DEVICE_OBJECT));
    if (device->wdm_device == NULL) {
        wdf_destroy_object(&device->object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(device->wdm_device, sizeof(DEVICE_OBJECT));
    device->wdm_device->Size = (USHORT)sizeof(DEVICE_OBJECT);
    device->wdm_device->DriverObject = driver != NULL ? driver->wdm_driver : NULL;
    device->wdm_device->DeviceExtension = device;
    if (driver != NULL && driver->wdm_driver != NULL) {
        device->wdm_device->NextDevice = driver->wdm_driver->DeviceObject;
        driver->wdm_driver->DeviceObject = device->wdm_device;
    }
    name[0] = 'W'; name[1] = 'd'; name[2] = 'f'; name[3] = 'D';
    name[4] = 'e'; name[5] = 'v'; name[6] = 'i'; name[7] = 'c';
    name[8] = 'e'; name[9] = '0'; name[10] = 0;
    name[9] = (char)('0' + (wdf_device_number++ % 10));
    device->io_device = IoCreateDevice(driver != NULL ? driver->io_driver : NULL, name, 0);
    if (device->io_device == NULL) {
        kfree(device->wdm_device);
        device->wdm_device = NULL;
        wdf_destroy_object(&device->object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    device->next_global = wdf_devices;
    wdf_devices = device;
    *device_handle = device;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_queue_create(PWDF_DRIVER_GLOBALS globals,
                                       WDFCORE_HANDLE device_handle,
                                       void *config,
                                       wdf_object_attributes *attributes,
                                       WDFCORE_HANDLE *queue_handle)
{
    wdf_queue *queue;
    (void)globals;
    if (device_handle == NULL || queue_handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (attributes == NULL) {
        wdf_object_attributes local = {0};
        local.ParentObject = device_handle;
        attributes = &local;
        queue = (wdf_queue *)wdf_new_object(WdfObjectQueue, sizeof(*queue), attributes);
    } else {
        queue = (wdf_queue *)wdf_new_object(WdfObjectQueue, sizeof(*queue), attributes);
    }
    if (queue == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    queue->device = (wdf_device *)wdf_from_handle(device_handle);
    queue->io_config = config;
    if (config != NULL) {
        wdf_io_queue_config *queue_config = (wdf_io_queue_config *)config;
        queue->evt_default = queue_config->EvtIoDefault;
        queue->evt_read = queue_config->EvtIoRead;
        queue->evt_write = queue_config->EvtIoWrite;
        queue->evt_device_control = queue_config->EvtIoDeviceControl;
        if (queue_config->DefaultQueue && queue->device != NULL) {
            queue->device->default_queue = queue;
        }
    }
    *queue_handle = queue;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_memory_create(PWDF_DRIVER_GLOBALS globals,
                                        wdf_object_attributes *attributes,
                                        POOL_TYPE pool_type,
                                        ULONG pool_tag,
                                        size_t size,
                                        WDFCORE_HANDLE *memory_handle,
                                        void **buffer)
{
    wdf_memory *memory;
    (void)globals;
    (void)pool_type;
    (void)pool_tag;
    if (size == 0 || memory_handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    memory = (wdf_memory *)wdf_new_object(WdfObjectMemory, sizeof(*memory), attributes);
    if (memory == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memory->buffer = kmalloc(size);
    if (memory->buffer == NULL) {
        wdf_destroy_object(&memory->object);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memory->size = size;
    memory->owns_buffer = TRUE;
    RtlZeroMemory(memory->buffer, size);
    if (buffer != NULL) {
        *buffer = memory->buffer;
    }
    *memory_handle = memory;
    return STATUS_SUCCESS;
}

static void *NTAPI wdf_memory_get_buffer(PWDF_DRIVER_GLOBALS globals,
                                         WDFCORE_HANDLE memory_handle,
                                         size_t *buffer_size)
{
    wdf_memory *memory = (wdf_memory *)wdf_from_handle(memory_handle);
    (void)globals;
    if (memory == NULL || memory->object.type != WdfObjectMemory) {
        return NULL;
    }
    if (buffer_size != NULL) {
        *buffer_size = memory->size;
    }
    return memory->buffer;
}

static NTSTATUS NTAPI wdf_memory_assign_buffer(PWDF_DRIVER_GLOBALS globals,
                                               WDFCORE_HANDLE memory_handle,
                                               void *buffer, size_t size)
{
    wdf_memory *memory = (wdf_memory *)wdf_from_handle(memory_handle);
    (void)globals;
    if (memory == NULL || memory->object.type != WdfObjectMemory || buffer == NULL || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    if (memory->owns_buffer && memory->buffer != NULL) {
        kfree(memory->buffer);
    }
    memory->buffer = buffer;
    memory->size = size;
    memory->owns_buffer = FALSE;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_memory_copy_to_buffer(PWDF_DRIVER_GLOBALS globals,
                                                WDFCORE_HANDLE memory_handle,
                                                size_t source_offset, void *buffer,
                                                size_t bytes)
{
    wdf_memory *memory = (wdf_memory *)wdf_from_handle(memory_handle);
    (void)globals;
    if (memory == NULL || buffer == NULL || source_offset > memory->size || bytes > memory->size - source_offset) {
        return STATUS_INVALID_PARAMETER;
    }
    RtlCopyMemory(buffer, (uint8_t *)memory->buffer + source_offset, bytes);
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_memory_copy_from_buffer(PWDF_DRIVER_GLOBALS globals,
                                                  WDFCORE_HANDLE memory_handle,
                                                  size_t destination_offset, const void *buffer,
                                                  size_t bytes)
{
    wdf_memory *memory = (wdf_memory *)wdf_from_handle(memory_handle);
    (void)globals;
    if (memory == NULL || buffer == NULL || destination_offset > memory->size || bytes > memory->size - destination_offset) {
        return STATUS_INVALID_PARAMETER;
    }
    RtlCopyMemory((uint8_t *)memory->buffer + destination_offset, (void *)buffer, bytes);
    return STATUS_SUCCESS;
}

static PDEVICE_OBJECT NTAPI wdf_device_wdm_get_device_object(PWDF_DRIVER_GLOBALS globals,
                                                              WDFCORE_HANDLE device_handle)
{
    wdf_device *device = (wdf_device *)wdf_from_handle(device_handle);
    (void)globals;
    return device != NULL ? device->wdm_device : NULL;
}

static WDFCORE_HANDLE NTAPI wdf_device_get_driver(PWDF_DRIVER_GLOBALS globals,
                                                  WDFCORE_HANDLE device_handle)
{
    wdf_device *device = (wdf_device *)wdf_from_handle(device_handle);
    (void)globals;
    return device != NULL && device->driver != NULL ? device->driver : NULL;
}

static void NTAPI wdf_object_delete(PWDF_DRIVER_GLOBALS globals, WDFCORE_HANDLE handle)
{
    wdf_object *object = wdf_from_handle(handle);
    (void)globals;
    if (object != NULL) {
        /* Explicit deletion is not held alive by parent or temporary refs. */
        wdf_destroy_object(object);
    }
}

static void NTAPI wdf_object_reference(PWDF_DRIVER_GLOBALS globals,
                                       WDFCORE_HANDLE handle,
                                       void *tag, LONG line, const char *file)
{
    (void)globals; (void)tag; (void)line; (void)file;
    wdf_reference(handle);
}

static void NTAPI wdf_object_dereference(PWDF_DRIVER_GLOBALS globals,
                                         WDFCORE_HANDLE handle,
                                         void *tag, LONG line, const char *file)
{
    (void)globals; (void)tag; (void)line; (void)file;
    wdf_dereference(handle);
}

static NTSTATUS NTAPI wdf_request_create(PWDF_DRIVER_GLOBALS globals,
                                         wdf_object_attributes *attributes,
                                         WDFCORE_HANDLE io_target,
                                         WDFCORE_HANDLE *request_handle)
{
    wdf_request *request;
    (void)globals;
    if (request_handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    request = (wdf_request *)wdf_new_object(WdfObjectRequest, sizeof(*request), attributes);
    if (request == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    request->io_target = io_target;
    request->status = STATUS_SUCCESS;
    *request_handle = request;
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI wdf_request_get_status(PWDF_DRIVER_GLOBALS globals,
                                             WDFCORE_HANDLE request_handle)
{
    wdf_request *request = (wdf_request *)wdf_from_handle(request_handle);
    (void)globals;
    return request != NULL ? request->status : STATUS_INVALID_PARAMETER;
}

static void NTAPI wdf_request_complete(PWDF_DRIVER_GLOBALS globals,
                                       WDFCORE_HANDLE request_handle,
                                       NTSTATUS status)
{
    wdf_request *request = (wdf_request *)wdf_from_handle(request_handle);
    (void)globals;
    if (request != NULL) {
        request->status = status;
        if (request->io_request != NULL) {
            wdf_io_request *io_request = (wdf_io_request *)request->io_request;
            io_request->io_status.status = status == STATUS_SUCCESS ? 0 : -3;
        }
    }
}

static int wdf_dispatch_wdm(wdf_device *device, PIRP irp, wdf_io_request *input)
{
    wdf_queue *queue;
    wdf_request *request;

    queue = device != NULL ? device->default_queue : NULL;
    if (queue == NULL || input == NULL) return -3;
    request = (wdf_request *)wdf_new_object(WdfObjectRequest, sizeof(*request), NULL);
    if (request == NULL) return -4;
    request->io_request = input;
    request->wdm_irp = irp;
    request->status = STATUS_SUCCESS;
    if (input->major_function == 2 && queue->evt_read != NULL) {
        ((void (*)(void *, void *, size_t))queue->evt_read)(queue, request, input->length);
    } else if (input->major_function == 3 && queue->evt_write != NULL) {
        ((void (*)(void *, void *, size_t))queue->evt_write)(queue, request, input->length);
    } else if (input->major_function == 4 && queue->evt_device_control != NULL) {
        ((void (*)(void *, void *, size_t, size_t, ULONG))queue->evt_device_control)(
            queue, request, input->length, input->parameters.device_control.input_length,
            input->parameters.device_control.code);
    } else if (queue->evt_default != NULL) {
        ((void (*)(void *, void *))queue->evt_default)(queue, request);
    } else {
        request->status = STATUS_INVALID_DEVICE_REQUEST;
    }
    if (irp != NULL) {
        irp->IoStatus.Status = request->status;
        irp->IoStatus.Information = input->io_status.information;
    }
    input->io_status.status = request->status == STATUS_SUCCESS ? 0 : -3;
    wdf_destroy_object(&request->object);
    return input->io_status.status;
}

static NTSTATUS NTAPI wdf_wdm_dispatch(PDEVICE_OBJECT device_object, PIRP irp)
{
    wdf_device *device = wdf_devices;
    wdf_io_request local_input;
    wdf_io_request *input;
    IO_STACK_LOCATION *stack;
    int status;

    while (device != NULL && device->wdm_device != device_object) device = device->next_global;
    if (device == NULL || irp == NULL || irp->CurrentStackLocation == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    input = (wdf_io_request *)irp->Tail;
    if (input == NULL) {
        stack = irp->CurrentStackLocation;
        RtlZeroMemory(&local_input, sizeof(local_input));
        local_input.buffer = irp->AssociatedIrp.UserBuffer;
        local_input.length = stack->MajorFunction == IRP_MJ_DEVICE_CONTROL
            ? stack->Parameters.DeviceIoControl.OutputBufferLength
            : (stack->MajorFunction == IRP_MJ_READ ? stack->Parameters.Read.Length : stack->Parameters.Write.Length);
        if (stack->MajorFunction == IRP_MJ_READ) local_input.major_function = 2;
        else if (stack->MajorFunction == IRP_MJ_WRITE) local_input.major_function = 3;
        else if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
            local_input.major_function = 4;
            local_input.parameters.device_control.code = stack->Parameters.DeviceIoControl.IoControlCode;
            local_input.parameters.device_control.input_buffer = stack->Parameters.DeviceIoControl.Type3InputBuffer;
            local_input.parameters.device_control.input_length = stack->Parameters.DeviceIoControl.InputBufferLength;
        } else {
            local_input.major_function = stack->MajorFunction == IRP_MJ_CLOSE ? 1 : 0;
        }
        input = &local_input;
    }
    status = wdf_dispatch_wdm(device, irp, input);
    return status == 0 ? STATUS_SUCCESS : irp->IoStatus.Status;
}

static int wdf_io_dispatch(void *io_device, void *io_request)
{
    wdf_device *device = wdf_devices;
    wdf_io_request *input = (wdf_io_request *)io_request;
    IRP irp;
    IO_STACK_LOCATION stack;
    UCHAR major;
    NTSTATUS status;

    while (device != NULL && device->io_device != io_device) device = device->next_global;
    if (device == NULL || input == NULL || device->wdm_device == NULL) return -3;
    RtlZeroMemory(&irp, sizeof(irp));
    RtlZeroMemory(&stack, sizeof(stack));
    irp.Type = (CSHORT)0x0006;
    irp.Size = (USHORT)sizeof(irp);
    irp.AssociatedIrp.UserBuffer = input->buffer;
    irp.Tail = input;
    irp.CurrentStackLocation = &stack;
    stack.DeviceObject = device->wdm_device;
    if (input->major_function == 2) major = IRP_MJ_READ;
    else if (input->major_function == 3) major = IRP_MJ_WRITE;
    else if (input->major_function == 4) major = IRP_MJ_DEVICE_CONTROL;
    else if (input->major_function == 1) major = IRP_MJ_CLOSE;
    else major = IRP_MJ_CREATE;
    stack.MajorFunction = major;
    if (major == IRP_MJ_READ) {
        stack.Parameters.Read.Length = input->length;
        stack.Parameters.Read.ByteOffset.QuadPart = input->parameters.read_write.offset;
    } else if (major == IRP_MJ_WRITE) {
        stack.Parameters.Write.Length = input->length;
        stack.Parameters.Write.ByteOffset.QuadPart = input->parameters.read_write.offset;
    } else if (major == IRP_MJ_DEVICE_CONTROL) {
        stack.Parameters.DeviceIoControl.IoControlCode = input->parameters.device_control.code;
        stack.Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)input->parameters.device_control.input_buffer;
        stack.Parameters.DeviceIoControl.InputBufferLength = input->parameters.device_control.input_length;
        stack.Parameters.DeviceIoControl.OutputBufferLength = input->length;
    }
    irp.IoStatus.Status = STATUS_SUCCESS;
    status = device->wdm_device->DriverObject->MajorFunction[major](device->wdm_device, &irp);
    input->io_status.status = status == STATUS_SUCCESS ? 0 : -3;
    input->io_status.information = (uint32_t)irp.IoStatus.Information;
    return input->io_status.status;
}

static NTSTATUS NTAPI wdf_unsupported(void)
{
    return STATUS_NOT_SUPPORTED;
}

static WDFFUNC wdf_function_table[WDF_FUNCTION_COUNT];
const WDFFUNC *WdfFunctions_01033 = wdf_function_table;
ULONG WdfFunctionCount = WDF_FUNCTION_COUNT;
WDFFUNC WdfDriverMiniportUnloadOverride = (WDFFUNC)0;

void WdfPlatformInitialize(void)
{
    size_t i;
    for (i = 0; i < WDF_FUNCTION_COUNT; ++i) {
        wdf_function_table[i] = (WDFFUNC)wdf_unsupported;
    }
    /* These values are the stable KMDF 1.33 indices from wdffuncenum.h. */
    wdf_function_table[31] = (WDFFUNC)wdf_device_wdm_get_device_object;
    wdf_function_table[39] = (WDFFUNC)wdf_device_get_driver;
    wdf_function_table[75] = (WDFFUNC)wdf_device_create;
    wdf_function_table[116] = (WDFFUNC)wdf_driver_create;
    wdf_function_table[152] = (WDFFUNC)wdf_queue_create;
    wdf_function_table[192] = (WDFFUNC)wdf_memory_create;
    wdf_function_table[194] = (WDFFUNC)wdf_memory_get_buffer;
    wdf_function_table[195] = (WDFFUNC)wdf_memory_assign_buffer;
    wdf_function_table[196] = (WDFFUNC)wdf_memory_copy_to_buffer;
    wdf_function_table[197] = (WDFFUNC)wdf_memory_copy_from_buffer;
    wdf_function_table[208] = (WDFFUNC)wdf_object_delete;
    wdf_function_table[205] = (WDFFUNC)wdf_object_reference;
    wdf_function_table[206] = (WDFFUNC)wdf_object_dereference;
    wdf_function_table[247] = (WDFFUNC)wdf_request_create;
    wdf_function_table[254] = (WDFFUNC)wdf_request_get_status;
    wdf_function_table[263] = (WDFFUNC)wdf_request_complete;
}

/* The kernel calls this during early initialization; the constructor is also
 * useful for hosted/unit-test builds of the platform layer. */
__attribute__((constructor)) static void wdf_platform_constructor(void)
{
    WdfPlatformInitialize();
}
