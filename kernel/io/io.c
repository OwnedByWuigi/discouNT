#include <stdint.h>
#include "io/io.h"
#include "mm/mm.h"
#include "core/util.h"
#include "serial.h"

static uint32_t driver_object_type;
static uint32_t device_object_type;
static IO_DRIVER_OBJECT *current_driver;

static void IoCopyName(char *destination, const char *source) {
    uint32_t length = strlen(source);
    if (length >= MAX_NAME_LEN) length = MAX_NAME_LEN - 1;
    memcpy(destination, source, length);
    destination[length] = 0;
}

static void IoDeleteDriverBody(void *body) {
    IO_DRIVER_OBJECT *driver = (IO_DRIVER_OBJECT*)body;
    while (driver && driver->device_list) IoDeleteDevice(driver->device_list);
    if (driver && driver->unload) driver->unload(driver);
}

static void IoDeleteDeviceBody(void *body) {
    IO_DEVICE_OBJECT *device = (IO_DEVICE_OBJECT*)body;
    if (device && device->device_extension) {
        kfree(device->device_extension);
        device->device_extension = 0;
    }
}

void IoInit(void) {
    driver_object_type = ObRegisterObjectType("IoDriver", IoDeleteDriverBody);
    device_object_type = ObRegisterObjectType("IoDevice", IoDeleteDeviceBody);
    current_driver = 0;
}

IO_DRIVER_OBJECT *IoCreateDriver(const char *name, void *image, void *context) {
    IO_DRIVER_OBJECT *driver;
    HANDLE handle;

    if (!driver_object_type || !name) return 0;
    driver = (IO_DRIVER_OBJECT*)kmalloc(sizeof(IO_DRIVER_OBJECT));
    if (!driver) return 0;
    memset(driver, 0, sizeof(*driver));
    driver->type = IO_TYPE_DRIVER;
    driver->image = image;
    driver->context = context;
    IoCopyName(driver->name, name);

    handle = ObCreateObject(driver_object_type, name, driver, sizeof(*driver));
    if (handle == INVALID_HANDLE) {
        kfree(driver);
        return 0;
    }
    driver->handle = handle;
    return driver;
}

IO_DRIVER_OBJECT *IoGetCurrentDriver(void) { return current_driver; }
void IoSetCurrentDriver(IO_DRIVER_OBJECT *driver) { current_driver = driver; }

void IoSetPlatformDispatch(IO_DRIVER_OBJECT *driver, uint32_t major_function,
                           IO_PLATFORM_DISPATCH dispatch) {
    if (!driver || major_function >= IO_MAX_MAJOR_FUNCTION) return;
    driver->platform_dispatch[major_function] = dispatch;
}

void IoDeleteDriver(IO_DRIVER_OBJECT *driver) {
    if (!driver || driver->type != IO_TYPE_DRIVER) return;
    if (current_driver == driver) current_driver = 0;
    ObDereferenceObject(driver->handle);
}

IO_DEVICE_OBJECT *IoCreateDevice(IO_DRIVER_OBJECT *driver, const char *name,
                                 uint32_t extension_size) {
    IO_DEVICE_OBJECT *device;
    HANDLE handle;

    if (!driver || driver->type != IO_TYPE_DRIVER || !name || !name[0]) return 0;
    if (ObFindObject(name, device_object_type) != INVALID_HANDLE) return 0;

    device = (IO_DEVICE_OBJECT*)kmalloc(sizeof(IO_DEVICE_OBJECT));
    if (!device) return 0;
    memset(device, 0, sizeof(*device));
    device->type = IO_TYPE_DEVICE;
    device->driver = driver;
    device->device_extension_size = extension_size;
    IoCopyName(device->name, name);

    if (extension_size) {
        device->device_extension = kmalloc(extension_size);
        if (!device->device_extension) {
            kfree(device);
            return 0;
        }
        memset(device->device_extension, 0, extension_size);
    }

    handle = ObCreateObject(device_object_type, name, device, sizeof(*device));
    if (handle == INVALID_HANDLE) {
        if (device->device_extension) kfree(device->device_extension);
        kfree(device);
        return 0;
    }
    device->handle = handle;
    device->next_device = driver->device_list;
    driver->device_list = device;
    return device;
}

IO_DEVICE_OBJECT *IoGetDevice(const char *name) {
    HANDLE handle;
    if (!name || !device_object_type) return 0;
    handle = ObFindObject(name, device_object_type);
    if (handle == INVALID_HANDLE) return 0;
    return (IO_DEVICE_OBJECT*)ObReferenceObject(handle);
}

void IoDeleteDevice(IO_DEVICE_OBJECT *device) {
    IO_DEVICE_OBJECT **link;
    if (!device || device->type != IO_TYPE_DEVICE) return;
    if (device->driver) {
        link = &device->driver->device_list;
        while (*link && *link != device) link = &(*link)->next_device;
        if (*link == device) *link = device->next_device;
    }
    ObDereferenceObject(device->handle);
}

int IoCallDriver(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    IO_DISPATCH_ROUTINE dispatch;
    int status;

    if (!device || device->type != IO_TYPE_DEVICE || !request)
        return IO_STATUS_INVALID_PARAMETER;
    if (!device->driver || request->major_function >= IO_MAX_MAJOR_FUNCTION)
        return IO_STATUS_INVALID_PARAMETER;

    dispatch = device->driver->major_function[request->major_function];
    if (!dispatch && device->driver->platform_dispatch[request->major_function]) {
        status = device->driver->platform_dispatch[request->major_function](device, request);
        request->io_status.status = status;
        return status;
    }
    if (!dispatch) {
        request->io_status.status = IO_STATUS_NOT_SUPPORTED;
        request->io_status.information = 0;
        return IO_STATUS_NOT_SUPPORTED;
    }
    request->io_status.status = IO_STATUS_DEVICE_ERROR;
    request->io_status.information = 0;
    status = dispatch(device, request);
    request->io_status.status = status;
    return status;
}

int IoSendRequest(const char *device_name, uint32_t major_function,
                  void *buffer, uint32_t length) {
    IO_DEVICE_OBJECT *device = IoGetDevice(device_name);
    IO_REQUEST request;
    int status;
    if (!device) return IO_STATUS_NOT_FOUND;
    memset(&request, 0, sizeof(request));
    request.major_function = major_function;
    request.buffer = buffer;
    request.length = length;
    status = IoCallDriver(device, &request);
    ObDereferenceObject(device->handle);
    return status;
}

int IoDeviceControl(const char *device_name, uint32_t code,
                    const void *input, uint32_t input_length,
                    void *output, uint32_t output_length,
                    uint32_t *information) {
    IO_DEVICE_OBJECT *device = IoGetDevice(device_name);
    IO_REQUEST request;
    int status;
    if (!device) return IO_STATUS_NOT_FOUND;
    memset(&request, 0, sizeof(request));
    request.major_function = IO_MJ_DEVICE_CONTROL;
    request.buffer = output;
    request.length = output_length;
    request.parameters.device_control.code = code;
    request.parameters.device_control.input_buffer = input;
    request.parameters.device_control.input_length = input_length;
    status = IoCallDriver(device, &request);
    if (information) *information = request.io_status.information;
    ObDereferenceObject(device->handle);
    return status;
}
