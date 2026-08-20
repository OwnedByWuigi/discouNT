#include <stdint.h>
#include "kexports.h"
#include "driver.h"
#include "mm.h"
#include "util.h"
#include "object.h"
#include "serial.h"
#include "cdfs.h"
#include "keyboard.h"
#include "mouse.h"
#include "net.h"
#include "fb.h"
#include "vga.h"
#include "w32k.h"
#include "ke.h"
#include "peloader.h"
#include "bugcheck.h"

extern void CsrssGinaShowLogon(void);
extern uint32_t KeGetProcessorCount(void);
extern uint32_t KeGetPhysicalMemoryPages(void);

typedef struct _KERNEL_EXPORT {
    const char *name;
    void *addr;
} KERNEL_EXPORT;

uint8_t back_buffer[640 * 480];

static KERNEL_EXPORT kernel_exports[] = {
    {"kmalloc", kmalloc},
    {"kfree", kfree},
    {"MmGetHeapUsed", MmGetHeapUsed},
    {"MmGetHeapTotal", MmGetHeapTotal},
    {"memset", memset},
    {"memcpy", memcpy},
    {"strlen", strlen},
    {"strcmp", strcmp},
    {"strcpy", strcpy},
    {"strcat", strcat},
    {"itoa", itoa},
    {"__udivdi3", __udivdi3},
    {"__umoddi3", __umoddi3},
    {"__divdi3", __divdi3},
    {"__moddi3", __moddi3},
    {"ObCreateObject", ObCreateObject},
    {"ObReferenceObject", ObReferenceObject},
    {"ObDereferenceObject", ObDereferenceObject},
    {"ObFindObject", ObFindObject},
    {"SerialInit", SerialInit},
    {"SerialPutChar", SerialPutChar},
    {"SerialPutString", SerialPutString},
    {"SerialPrintHex", SerialPrintHex},
    {"SerialPrintDec", SerialPrintDec},
    {"CdfsInit", CdfsInit},
    {"CdfsReadSector", CdfsReadSector},
    {"CdfsFindFile", CdfsFindFile},
    {"CdfsReadFile", CdfsReadFile},
    {"KeyboardInit", KeyboardInit},
    {"KeyboardHandleData", KeyboardHandleData},
    {"KeyboardHandleControllerEvent", KeyboardHandleControllerEvent},
    {"KeyboardPollEvent", KeyboardPollEvent},
    {"MouseInit", MouseInit},
    {"MouseGetState", MouseGetState},
    {"MouseDrawCursor", MouseDrawCursor},
    {"MouseEraseCursor", MouseEraseCursor},
    {"MouseSetCursorType", MouseSetCursorType},
    {"MouseHandleByte", MouseHandleByte},
    {"MouseHandleInterrupt", MouseHandleInterrupt},
    {"NetInit", NetInit},
    {"NetPoll", NetPoll},
    {"NetIsReady", NetIsReady},
    {"NetPing", NetPing},
    {"VgaInit", VgaInit},
    {"VgaClearScreen", VgaClearScreen},
    {"VgaPutPixel", VgaPutPixel},
    {"VgaFillRect", VgaFillRect},
    {"VgaDrawRect", VgaDrawRect},
    {"VgaDrawChar", VgaDrawChar},
    {"VgaDrawString", VgaDrawString},
    {"VgaSwapBuffers", VgaSwapBuffers},
    {"back_buffer", back_buffer},
    {"FbInit", FbInit},
    {"FbClearScreen", FbClearScreen},
    {"FbPutPixel", FbPutPixel},
    {"FbFillRect", FbFillRect},
    {"FbFillRectRGB", FbFillRectRGB},
    {"FbDrawRect", FbDrawRect},
    {"FbDrawChar", FbDrawChar},
    {"FbDrawString", FbDrawString},
    {"FbSwapBuffers", FbSwapBuffers},
    {"FbIsFramebuffer", FbIsFramebuffer},
    {"FbGetWidth", FbGetWidth},
    {"FbGetHeight", FbGetHeight},
    {"FbGetModeCount", FbGetModeCount},
    {"FbGetModeInfo", FbGetModeInfo},
    {"FbSetResolution", FbSetResolution},
    {"FbGetPixel", FbGetPixel},
    {"FbCapture", FbCapture},
    {"FbBlitIndexed", FbBlitIndexed},
    {"Win32kInit", Win32kInit},
    {"Win32kRegisterClass", Win32kRegisterClass},
    {"Win32kCreateWindow", Win32kCreateWindow},
    {"Win32kCreateWindowByClass", Win32kCreateWindowByClass},
    {"Win32kShowWindow", Win32kShowWindow},
    {"Win32kUpdateWindow", Win32kUpdateWindow},
    {"Win32kGetClientRect", Win32kGetClientRect},
    {"Win32kGetWindowRect", Win32kGetWindowRect},
    {"Win32kDestroyWindow", Win32kDestroyWindow},
    {"Win32kHandleMouseDown", Win32kHandleMouseDown},
    {"Win32kHandleMouseUp", Win32kHandleMouseUp},
    {"Win32kHandleMouseMove", Win32kHandleMouseMove},
    {"Win32kRedrawAll", Win32kRedrawAll},
    {"Win32kSetColorPreview", Win32kSetColorPreview},
    {"Win32kRefreshCursor", Win32kRefreshCursor},
    {"Win32kIsDragging", Win32kIsDragging},
    {"Win32kIsResizing", Win32kIsResizing},
    {"Win32kGetActiveWindow", Win32kGetActiveWindow},
    {"Win32kActivateWindow", Win32kActivateWindow},
    {"KeAttachCurrentThread", KeAttachCurrentThread},
    {"KeCreateThread", KeCreateThread},
    {"KeYield", KeYield},
    {"KeGetSchedulerTicks", KeGetSchedulerTicks},
    {"KeGetProcessorCount", KeGetProcessorCount},
    {"KeGetPhysicalMemoryPages", KeGetPhysicalMemoryPages},
    {"KeCreateEvent", KeCreateEvent},
    {"KeSetEvent", KeSetEvent},
    {"KeResetEvent", KeResetEvent},
    {"KeWaitEvent", KeWaitEvent},
    {"KeBugCheck", KeBugCheck},
    {"KeBugCheckEx", KeBugCheckEx},
    {"PeGetLoadedModuleHandle", PeGetLoadedModuleHandle},
    {"PeGetImagePath", PeGetImagePath},
    {"PeLoadDll", PeLoadDll},
    {"PeGetProcAddress", PeGetProcAddress},
    {"PeResolveExternalSymbol", PeResolveExternalSymbol},
    {"CsrssGinaShowLogon", CsrssGinaShowLogon},
};

void *KernelResolveSymbol(const char *name) {
    void *resolved;

    if (!name || !*name) return 0;

    for (uint32_t i = 0; i < (sizeof(kernel_exports) / sizeof(kernel_exports[0])); i++) {
        if (strcmp(kernel_exports[i].name, name) == 0) {
            return kernel_exports[i].addr;
        }
    }

    resolved = PeResolveExternalSymbol(name);
    if (resolved) return resolved;

    return DriverResolveSymbol(name);
}
