#include <stdint.h>

typedef void *HMODULE;
typedef void *PVOID;
typedef int BOOL;

extern HMODULE LoadLibraryA(const char *name);
extern PVOID GetProcAddress(HMODULE module, const char *name);
extern BOOL FreeLibrary(HMODULE module);
extern void Sleep(uint32_t milliseconds);

typedef int (*UMDF_ENTRY)(PVOID driver_object, PVOID registry_path);
typedef int (*UMDF_RUN)(void);
typedef int (*UMDF_PUMP)(void);

int main(int argc, char **argv)
{
    HMODULE module;
    HMODULE runtime;
    UMDF_ENTRY entry;
    UMDF_PUMP pump;

    if (argc < 2 || argv[1] == 0) return 2;
    module = LoadLibraryA(argv[1]);
    if (!module) return 3;

    entry = (UMDF_ENTRY)GetProcAddress(module, "DriverEntry");
    if (!entry) entry = (UMDF_ENTRY)GetProcAddress(module, "WudfDriverEntry");
    if (!entry || entry(0, 0) != 0) {
        FreeLibrary(module);
        return 4;
    }

    runtime = LoadLibraryA("WUDFRD.DLL");
    pump = runtime ? (UMDF_PUMP)GetProcAddress(runtime, "WudfHostPump") : 0;
    for (;;) {
        if (pump) pump();
        Sleep(1);
    }
}
