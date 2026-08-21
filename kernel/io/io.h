#ifndef IO_H
#define IO_H

#include <stdint.h>
#include "ob/object.h"

#define IO_MAX_MAJOR_FUNCTION 8
#define IO_TYPE_DRIVER 0x44525652U
#define IO_TYPE_DEVICE 0x44455643U

#define IO_STATUS_SUCCESS           0
#define IO_STATUS_INVALID_PARAMETER (-1)
#define IO_STATUS_NOT_FOUND         (-2)
#define IO_STATUS_NOT_SUPPORTED     (-3)
#define IO_STATUS_NO_MEMORY         (-4)
#define IO_STATUS_DEVICE_ERROR      (-5)

enum {
    IO_MJ_CREATE = 0,
    IO_MJ_CLOSE,
    IO_MJ_READ,
    IO_MJ_WRITE,
    IO_MJ_DEVICE_CONTROL,
    IO_MJ_FLUSH,
    IO_MJ_START,
    IO_MJ_STOP
};

typedef struct _IO_DRIVER_OBJECT IO_DRIVER_OBJECT;
typedef struct _IO_DEVICE_OBJECT IO_DEVICE_OBJECT;
typedef struct _IO_REQUEST IO_REQUEST;

typedef int (*IO_DISPATCH_ROUTINE)(IO_DEVICE_OBJECT *device, IO_REQUEST *request);
typedef void (*IO_DRIVER_UNLOAD)(IO_DRIVER_OBJECT *driver);

typedef struct _IO_STATUS_BLOCK {
    int status;
    uint32_t information;
} IO_STATUS_BLOCK;

struct _IO_REQUEST {
    uint32_t major_function;
    IO_STATUS_BLOCK io_status;
    void *buffer;
    uint32_t length;
    union {
        struct { uint64_t offset; } read_write;
        struct {
            uint32_t code;
            const void *input_buffer;
            uint32_t input_length;
        } device_control;
    } parameters;
};

struct _IO_DRIVER_OBJECT {
    uint32_t type;
    char name[MAX_NAME_LEN];
    void *image;
    void *context;
    IO_DISPATCH_ROUTINE major_function[IO_MAX_MAJOR_FUNCTION];
    IO_DRIVER_UNLOAD unload;
    IO_DEVICE_OBJECT *device_list;
    HANDLE handle;
};

struct _IO_DEVICE_OBJECT {
    uint32_t type;
    char name[MAX_NAME_LEN];
    IO_DRIVER_OBJECT *driver;
    void *device_extension;
    uint32_t device_extension_size;
    IO_DEVICE_OBJECT *next_device;
    HANDLE handle;
};

void IoInit(void);
IO_DRIVER_OBJECT *IoCreateDriver(const char *name, void *image, void *context);
void IoDeleteDriver(IO_DRIVER_OBJECT *driver);
IO_DRIVER_OBJECT *IoGetCurrentDriver(void);
void IoSetCurrentDriver(IO_DRIVER_OBJECT *driver);
IO_DEVICE_OBJECT *IoCreateDevice(IO_DRIVER_OBJECT *driver, const char *name,
                                 uint32_t extension_size);
IO_DEVICE_OBJECT *IoGetDevice(const char *name);
void IoDeleteDevice(IO_DEVICE_OBJECT *device);
int IoCallDriver(IO_DEVICE_OBJECT *device, IO_REQUEST *request);
int IoSendRequest(const char *device_name, uint32_t major_function,
                  void *buffer, uint32_t length);
int IoDeviceControl(const char *device_name, uint32_t code,
                    const void *input, uint32_t input_length,
                    void *output, uint32_t output_length,
                    uint32_t *information);

#endif
