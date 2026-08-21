#include <stdint.h>
#include "io/driver.h"
#include "io/service.h"
#include "io/io.h"
#include "cdfs.h"
#include "mm/mm.h"
#include "core/util.h"
#include "serial.h"
#include "loader/peloader.h"

typedef struct _LOADED_DRIVER {
    char path[64];
    void *image;
} LOADED_DRIVER;

static LOADED_DRIVER g_loaded_drivers[16];
static int g_loaded_driver_count = 0;

void *DriverResolveSymbol(const char *name) {
    if (!name) return 0;

    for (int i = g_loaded_driver_count - 1; i >= 0; i--) {
        void *addr = PeGetProcAddress(g_loaded_drivers[i].image, name);
        if (addr) return addr;
    }
    return 0;
}

static int driver_call_entry(const char *name, void *image, void *context) {
    typedef int (*DriverEntryFn)(IO_DRIVER_OBJECT *driver, void *context);
    DriverEntryFn entry = (DriverEntryFn)PeGetProcAddress(image, "DriverEntry");
    IO_DRIVER_OBJECT *driver;
    int status;
    driver = IoCreateDriver(name, image, context);
    if (!driver) return 0;
    if (!entry) return 1;
    IoSetCurrentDriver(driver);
    status = entry(driver, context);
    IoSetCurrentDriver(0);
    if (!status) IoDeleteDriver(driver);
    return status;
}

static int driver_register_image(const char *path, void *image) {
    if (g_loaded_driver_count >= 16) return 0;
    strcpy(g_loaded_drivers[g_loaded_driver_count].path, path);
    g_loaded_drivers[g_loaded_driver_count].image = image;
    g_loaded_driver_count++;
    return 1;
}

static int driver_load_one(const SERVICE_DESCRIPTOR *service, void *context) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;

    /* Built-in boot services can participate in dependency ordering without
       having an image on the system volume. */
    if (!service->image_path) {
        SerialPutString("[DRV] Started built-in service ");
        SerialPutString(service->name);
        SerialPutString("\r\n");
        return 1;
    }

    if (!CdfsReadFile(service->image_path, &file_buf, &file_size)) {
        SerialPutString("[DRV] Missing ");
        SerialPutString(service->image_path);
        SerialPutString("\r\n");
        return 0;
    }

    image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    if (!image) {
        SerialPutString("[DRV] Load failed ");
        SerialPutString(service->image_path);
        SerialPutString("\r\n");
        return 0;
    }

    if (!PeResolveImports(image)) {
        SerialPutString("[DRV] Import resolution failed: ");
        if (PeGetLastError()) SerialPutString(PeGetLastError());
        SerialPutString("\r\n");
        PeFreeImage(image);
        return 0;
    }
    if (*(uint32_t*)image != 0x464C457F) {
        PePerformRelocations(image);
    }

    driver_register_image(service->image_path, image);

    if (!driver_call_entry(service->name, image, context)) {
        SerialPutString("[DRV] DriverEntry failed ");
        SerialPutString(service->image_path);
        SerialPutString("\r\n");
        return 0;
    }

    if (strcmp(service->name, "Win32k") == 0) DriverInstallWin32k(image);
    else if (strcmp(service->name, "Serial") == 0) DriverInstallSerial(image);
    else if (strcmp(service->name, "Vga") == 0) DriverInstallVga(image);
    else if (strcmp(service->name, "Cdfs") == 0) DriverInstallCdfs(image);
    else if (strcmp(service->name, "Keyboard") == 0) DriverInstallKeyboard(image);
    else if (strcmp(service->name, "Mouse") == 0) DriverInstallMouse(image);
    else if (strcmp(service->name, "Net") == 0) DriverInstallNet(image);
    else if (strcmp(service->name, "Framebuffer") == 0) DriverInstallFb(image);

    SerialPutString("[DRV] Loaded ");
    SerialPutString(service->image_path);
    SerialPutString("\r\n");
    return 1;
}

void DriverLoadAll(void *mb_info) {
#if defined(__loongarch64)
    /* LA64 links its boot drivers directly until module relocations cover the
       complete LoongArch ABI.  User-mode DLLs still use DriverResolveSymbol. */
    (void)mb_info;
#else
    static const char *cdfs_deps[] = {"Storage"};
    static const char *input_deps[] = {"Serial"};
    static const char *mouse_deps[] = {"Keyboard", "Framebuffer"};
    static const char *win32k_deps[] = {"Framebuffer", "Mouse"};
    static const SERVICE_DESCRIPTOR services[] = {
        {"Storage", 0, SERVICE_KERNEL_DRIVER, SERVICE_BOOT_START, "Boot Bus Extender", 0, 0},
        {"Serial", "/SYSTEM32/DRIVERS/SERIAL.SYS", SERVICE_KERNEL_DRIVER, SERVICE_BOOT_START, "System Bus Extender", 0, 0},
        {"Cdfs", "/SYSTEM32/DRIVERS/CDFS.SYS", SERVICE_FILE_SYSTEM_DRIVER, SERVICE_SYSTEM_START, "File System", cdfs_deps, 1},
        {"Vga", "/SYSTEM32/DRIVERS/VGA.SYS", SERVICE_KERNEL_DRIVER, SERVICE_SYSTEM_START, "Video", 0, 0},
        {"Keyboard", "/SYSTEM32/DRIVERS/KEYBOARD.SYS", SERVICE_KERNEL_DRIVER, SERVICE_SYSTEM_START, "Input", input_deps, 1},
        {"Framebuffer", "/SYSTEM32/DRIVERS/FB.SYS", SERVICE_KERNEL_DRIVER, SERVICE_SYSTEM_START, "Video", 0, 0},
        {"Mouse", "/SYSTEM32/DRIVERS/MOUSE.SYS", SERVICE_KERNEL_DRIVER, SERVICE_SYSTEM_START, "Input", mouse_deps, 2},
        {"Net", "/SYSTEM32/DRIVERS/NET.SYS", SERVICE_KERNEL_DRIVER, SERVICE_AUTO_START, "Network", 0, 0},
        {"Win32k", "/SYSTEM32/WIN32K.DLL", SERVICE_WIN32_SUBSYSTEM, SERVICE_AUTO_START, "Win32", win32k_deps, 2}
    };
    ServiceManagerInitialize();
    ServiceManagerStart(services, sizeof(services) / sizeof(services[0]), driver_load_one, mb_info);
#endif
}
