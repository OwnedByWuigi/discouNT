#include <stdint.h>
#include "arch/x86/hal.h"
#include "mm/mm.h"
#include "ob/object.h"
#include "io/io.h"
#include "core/ke.h"
#include "serial.h"
#include "cdfs.h"
#include "core/subsystem.h"
#include "keyboard.h"
#include "net.h"
#include "io/driver.h"
#include "core/util.h"
#include "core/version.h"
#include "arch/x86/idt.h"
#include "arch/x86/multiboot.h"
#include "usb.h"
#include "ide.h"
#include "fat32.h"
#include "core/setup.h"

static int BootOptionRequested(void *mb_info_ptr, const char *option) {
    MULTIBOOT_INFO *mbi = (MULTIBOOT_INFO*)mb_info_ptr;
    const char *cmdline;

    if (!mbi || !(mbi->flags & (1U << 2)) || !mbi->cmdline) return 0;
    cmdline = (const char*)(uintptr_t)mbi->cmdline;

    while (*cmdline) {
        while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
        const char *word = cmdline;
        const char *wanted = option;
        while (*word && *wanted && *word == *wanted) { word++; wanted++; }
        if (!*wanted && (*word == 0 || *word == ' ' || *word == '\t')) return 1;
        while (*cmdline && *cmdline != ' ' && *cmdline != '\t') cmdline++;
    }
    return 0;
}

static uint32_t g_cpu_core_count = 1;
static uint32_t g_physical_pages = 16384;

static void BootPutAt(int col, int row, const char *text, uint8_t color) {
    HalSetCursor(col, row);
    HalPutString(text, color);
}

static uint32_t DetectCpuCoreCount(void) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t max_basic;
    uint32_t logical_count = 1;
    uint32_t core_count = 1;

    __asm__ volatile(
        "cpuid"
        : "=a"(max_basic), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    if (max_basic >= 1) {
        __asm__ volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(1)
        );
        logical_count = (ebx >> 16) & 0xFF;
        if (logical_count == 0) logical_count = 1;
    }

    if (max_basic >= 4) {
        __asm__ volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(4), "c"(0)
        );
        core_count = ((eax >> 26) & 0x3F) + 1;
        if (core_count == 0) core_count = 1;
    } else {
        core_count = logical_count;
    }

    if (logical_count > core_count) return logical_count;
    return core_count;
}

static uint32_t DetectRamMb(void *mb_info_ptr) {
    MULTIBOOT_INFO *mbi = (MULTIBOOT_INFO*)mb_info_ptr;
    uint32_t total_kb;

    if (!mbi) return 0;
    if (!(mbi->flags & 0x1)) return 0;

    total_kb = mbi->mem_lower + mbi->mem_upper;
    if (total_kb == 0) return 0;

    return (total_kb + 1023) / 1024;
}

uint32_t KeGetProcessorCount(void) { return g_cpu_core_count; }
uint32_t KeGetPhysicalMemoryPages(void) { return g_physical_pages; }

static void ShowBootScreen(void *mb_info_ptr) {
    char line2[64];
    char numbuf[16];
    uint32_t cpu_cores = DetectCpuCoreCount();
    uint32_t ram_mb = DetectRamMb(mb_info_ptr);
    MULTIBOOT_INFO *mbi = (MULTIBOOT_INFO*)mb_info_ptr;
    g_cpu_core_count = cpu_cores ? cpu_cores : 1;
    if (mbi && (mbi->flags & 1U)) {
        uint32_t total_kb = mbi->mem_lower + mbi->mem_upper;
        if (total_kb) g_physical_pages = ((total_kb * 1024U) + 4095U) / 4096U;
    }

    HalClearScreen(0x1F);
    BootPutAt(0, 0, DISCOUNT_NAME " (Version " DISCOUNT_VERSION ")", 0x1F);

    line2[0] = 0;
    itoa((int)cpu_cores, numbuf, 10);
    strcat(line2, numbuf);
    strcat(line2, (cpu_cores == 1) ? " CPU core, " : " CPU cores, ");
    if (ram_mb == 0) {
        strcat(line2, "unknown RAM");
    } else {
        itoa((int)ram_mb, numbuf, 10);
        strcat(line2, numbuf);
        strcat(line2, " MB RAM");
    }

    BootPutAt(0, 1, line2, 0x1F);
}

void kmain(uint32_t magic, void *mb_info_ptr) {
    int screen_debug;
    int setup_mode;
    (void)magic;

    screen_debug = BootOptionRequested(mb_info_ptr, "screen-debug");
    setup_mode = BootOptionRequested(mb_info_ptr, "setup");
    SerialSetDebugEnabled(BootOptionRequested(mb_info_ptr, "debug"));
    SerialSetScreenDebugEnabled(screen_debug);
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  " DISCOUNT_NAME "\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    HalConfigureBootDisplay(mb_info_ptr);
    HalInitialize();
    MmInitialize(mb_info_ptr);
    IdtInitialize();
    /* The normal status screen clears VGA text memory.  In screen-debug mode
       that memory is the debug console, so leave its log intact. */
    if (!screen_debug) ShowBootScreen(mb_info_ptr);
    
    ObInit();
    IoInit();
    KeInit();
    KeAttachCurrentThread("KernelMain");
    if (IdeBootInitialize() && !setup_mode) Fat32Initialize("Harddisk0");
    if (UsbBootInitialize() && !Fat32IsMounted() && !setup_mode) Fat32Initialize("UsbDisk0");
    if (!Fat32IsMounted()) CdfsInit();
    DriverLoadAll(mb_info_ptr);
    KeyboardInit();
    if (setup_mode) SetupRun();
#if defined(__x86_64__)
    /* The current NIC backends still contain 32-bit MMIO/PCI assumptions.
       Do not let their probe trap or stall the native boot before CSRSS. */
    SerialPutString("[NET] AMD64 NIC probe deferred\r\n");
#else
    SerialPutString("[BOOT] Starting network initialization\r\n");
    NetInit();
    SerialPutString("[BOOT] Network initialization complete\r\n");
#endif
    SubsystemInit(mb_info_ptr);
    SubsystemLaunchSmss();

    for (;;) __asm__ volatile("hlt");
}
