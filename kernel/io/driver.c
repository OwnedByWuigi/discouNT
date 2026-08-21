#include <stdint.h>
#include "io/driver.h"
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

typedef struct _DRIVER_SPEC {
    const char *path;
    void (*install)(void *image);
} DRIVER_SPEC;

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

static int driver_load_one(const DRIVER_SPEC *spec, void *context) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;

    if (!CdfsReadFile(spec->path, &file_buf, &file_size)) {
        SerialPutString("[DRV] Missing ");
        SerialPutString(spec->path);
        SerialPutString("\r\n");
        return 0;
    }

    image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    if (!image) {
        SerialPutString("[DRV] Load failed ");
        SerialPutString(spec->path);
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

    driver_register_image(spec->path, image);

    if (!driver_call_entry(spec->path, image, context)) {
        SerialPutString("[DRV] DriverEntry failed ");
        SerialPutString(spec->path);
        SerialPutString("\r\n");
        return 0;
    }

    if (spec->install) spec->install(image);

    SerialPutString("[DRV] Loaded ");
    SerialPutString(spec->path);
    SerialPutString("\r\n");
    return 1;
}

static int driver_load_w32k_dll(void) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;
    const char *path = "/SYSTEM32/WIN32K.DLL";

    if (!CdfsReadFile(path, &file_buf, &file_size)) {
        SerialPutString("[DRV] Missing ");
        SerialPutString(path);
        SerialPutString("\r\n");
        return 0;
    }

    image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    if (!image) {
        SerialPutString("[DRV] Load failed ");
        SerialPutString(path);
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

    driver_register_image(path, image);
    DriverInstallWin32k(image);
    SerialPutString("[DRV] Loaded ");
    SerialPutString(path);
    SerialPutString("\r\n");
    return 1;
}

void DriverLoadAll(void *mb_info) {
#if defined(__loongarch64)
    /* LA64 links its boot drivers directly until module relocations cover the
       complete LoongArch ABI.  User-mode DLLs still use DriverResolveSymbol. */
    (void)mb_info;
#else
    static const DRIVER_SPEC specs[] = {
        {"/SYSTEM32/DRIVERS/SERIAL.SYS", DriverInstallSerial},
        {"/SYSTEM32/DRIVERS/VGA.SYS", DriverInstallVga},
        {"/SYSTEM32/DRIVERS/CDFS.SYS", DriverInstallCdfs},
        {"/SYSTEM32/DRIVERS/KEYBOARD.SYS", DriverInstallKeyboard},
        {"/SYSTEM32/DRIVERS/MOUSE.SYS", DriverInstallMouse},
        {"/SYSTEM32/DRIVERS/NET.SYS", DriverInstallNet},
        {"/SYSTEM32/DRIVERS/FB.SYS", DriverInstallFb},
    };

    for (uint32_t i = 0; i < (sizeof(specs) / sizeof(specs[0])); i++) {
        driver_load_one(&specs[i], mb_info);
    }

    driver_load_w32k_dll();
#endif
}
