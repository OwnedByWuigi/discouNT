#include <stdint.h>
#include "core/kexports.h"
#include "io/driver.h"
#include "mm/mm.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "core/util.h"
#include "rtl/rtlpath.h"
#include "ob/object.h"
#include "io/io.h"
#include "serial.h"
#include "cdfs.h"
#include "fat32.h"
#include "keyboard.h"
#include "mouse.h"
#include "net.h"
#include "fb.h"
#include "vga.h"
#include "w32k.h"
#include "core/ke.h"
#include "loader/peloader.h"
#include "core/bugcheck.h"

extern void CsrssGinaShowLogon(void);
extern int CsrssExecuteImage(const char *path);
extern void CsrssShutdownSystem(void);
extern uint32_t KeGetProcessorCount(void);
extern uint32_t KeGetPhysicalMemoryPages(void);

typedef struct {
    uint16_t vt, reserved1, reserved2, reserved3;
    union { int32_t lval; void *ptr; } value;
} KERNEL_VARIANT;

/* Native ELF images do not retain the DLL name associated with an undefined
   symbol.  Their imports are therefore resolved through this kernel export
   table.  Keep the small OLE Automation primitives here as well as in
   oleaut32 so callers cannot receive null IAT entries when oleaut32 has not
   already been loaded. */
void *SysAllocString(const uint32_t *text)
{
    uint32_t length = 0;
    uint32_t *copy;

    while (text && text[length]) length++;
    copy = (uint32_t *)kmalloc((uint32_t)((length + 1) * sizeof(uint32_t)));
    if (!copy) return 0;
    if (length) memcpy(copy, text, (uint32_t)(length * sizeof(uint32_t)));
    copy[length] = 0;
    return copy;
}

void SysFreeString(void *text)
{
    kfree(text);
}

void VariantInit(void *argument)
{
    if (argument) memset(argument, 0, sizeof(KERNEL_VARIANT));
}

int32_t VariantClear(void *argument)
{
    KERNEL_VARIANT *value = (KERNEL_VARIANT *)argument;
    if (!value) return (int32_t)0x80070057;
    if (value->vt == 8 && value->value.ptr) kfree(value->value.ptr);
    VariantInit(value);
    return 0;
}

typedef struct _KERNEL_EXPORT {
    const char *name;
    void *addr;
} KERNEL_EXPORT;

extern uint8_t back_buffer[640 * 480];

static KERNEL_EXPORT kernel_exports[] = {
    {"kmalloc", kmalloc},
    {"kfree", kfree},
    {"SysAllocString", SysAllocString},
    {"SysFreeString", SysFreeString},
    {"VariantInit", VariantInit},
    {"VariantClear", VariantClear},
    {"malloc", malloc},
    {"calloc", calloc},
    {"realloc", realloc},
    {"free", free},
    {"MmGetHeapUsed", MmGetHeapUsed},
    {"MmGetHeapTotal", MmGetHeapTotal},
    {"PmmAllocatePage", PmmAllocatePage},
    {"PmmAllocatePages", PmmAllocatePages},
    {"PmmFreePage", PmmFreePage},
    {"PmmFreePages", PmmFreePages},
    {"PmmGetTotalPages", PmmGetTotalPages},
    {"PmmGetFreePages", PmmGetFreePages},
    {"VmmAllocatePages", VmmAllocatePages},
    {"VmmFreePages", VmmFreePages},
    {"VmmGetPhysicalAddress", VmmGetPhysicalAddress},
    {"VmmMapMmioRange", VmmMapMmioRange},
    {"memset", memset},
    {"memcpy", memcpy},
    {"memmove", memmove},
    {"wcstoul", wcstoul},
    {"strlen", strlen},
    {"strcmp", strcmp},
    {"strcpy", strcpy},
    {"strcat", strcat},
    {"itoa", itoa},
    {"RtlNormalizePath", RtlNormalizePath},
    {"RtlNextPathComponent", RtlNextPathComponent},
    {"RtlJoinPath", RtlJoinPath},
    {"RtlPathFileName", RtlPathFileName},
    {"RtlReplacePathExtension", RtlReplacePathExtension},
    {"__udivdi3", __udivdi3},
    {"__umoddi3", __umoddi3},
    {"__divdi3", __divdi3},
    {"__moddi3", __moddi3},
    {"ObCreateObject", ObCreateObject},
    {"ObRegisterObjectType", ObRegisterObjectType},
    {"ObGetObjectType", ObGetObjectType},
    {"ObReferenceObject", ObReferenceObject},
    {"ObDereferenceObject", ObDereferenceObject},
    {"ObFindObject", ObFindObject},
    {"IoCreateDevice", IoCreateDevice},
    {"IoDeleteDriver", IoDeleteDriver},
    {"IoGetDevice", IoGetDevice},
    {"IoDeleteDevice", IoDeleteDevice},
    {"IoCallDriver", IoCallDriver},
    {"IoSendRequest", IoSendRequest},
    {"IoDeviceControl", IoDeviceControl},
    {"SerialInit", SerialInit},
    {"SerialPutChar", SerialPutChar},
    {"SerialPutString", SerialPutString},
    {"SerialPrintHex", SerialPrintHex},
    {"SerialPrintDec", SerialPrintDec},
    {"CdfsInit", CdfsInit},
    {"CdfsReadSector", CdfsReadSector},
    {"CdfsFindFile", CdfsFindFile},
    {"CdfsReadFile", CdfsReadFile},
    {"Fat32Initialize", Fat32Initialize},
    {"Fat32IsMounted", Fat32IsMounted},
    {"Fat32ReadFile", Fat32ReadFile},
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
    {"FbGetPixelRGB", FbGetPixelRGB},
    {"FbPutPixelRGB", FbPutPixelRGB},
    {"FbPaintWallpaper", FbPaintWallpaper},
    {"FbCaptureRGB", FbCaptureRGB},
    {"FbBlitRGB", FbBlitRGB},
    {"FbSetClipRect", FbSetClipRect},
    {"FbResetClipRect", FbResetClipRect},
    {"FbCapture", FbCapture},
    {"FbBlitIndexed", FbBlitIndexed},
    {"Win32kInit", Win32kInit},
    {"Win32kRegisterClass", Win32kRegisterClass},
    {"Win32kCreateWindow", Win32kCreateWindow},
    {"Win32kCreateWindowByClass", Win32kCreateWindowByClass},
    {"Win32kShowWindow", Win32kShowWindow},
    {"Win32kUpdateWindow", Win32kUpdateWindow},
    {"Win32kGetClientRect", Win32kGetClientRect},
    {"Win32kGetClientScreenRect", Win32kGetClientScreenRect},
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
    {"Win32kSetWindowIcons", Win32kSetWindowIcons},
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
    {"CsrssExecuteImage", CsrssExecuteImage},
    {"CsrssShutdownSystem", CsrssShutdownSystem},
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
