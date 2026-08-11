#include <stdint.h>
#include "hal.h"
#include "object.h"
#include "ke.h"
#include "serial.h"
#include "cdfs.h"
#include "subsystem.h"
#include "keyboard.h"
#include "net.h"
#include "driver.h"
#include "util.h"
#include "version.h"

typedef struct _MULTIBOOT_INFO {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
} MULTIBOOT_INFO;

static int BootDebugRequested(void *mb_info_ptr) {
    MULTIBOOT_INFO *mbi = (MULTIBOOT_INFO*)mb_info_ptr;
    const char *cmdline;

    if (!mbi || !(mbi->flags & (1U << 2)) || !mbi->cmdline) return 0;
    cmdline = (const char*)(uintptr_t)mbi->cmdline;

    while (*cmdline) {
        while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
        if (cmdline[0] == 'd' && cmdline[1] == 'e' &&
            cmdline[2] == 'b' && cmdline[3] == 'u' &&
            cmdline[4] == 'g' &&
            (cmdline[5] == 0 || cmdline[5] == ' ' || cmdline[5] == '\t')) {
            return 1;
        }
        while (*cmdline && *cmdline != ' ' && *cmdline != '\t') cmdline++;
    }
    return 0;
}

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

static void ShowBootScreen(void *mb_info_ptr) {
    char line2[64];
    char numbuf[16];
    uint32_t cpu_cores = DetectCpuCoreCount();
    uint32_t ram_mb = DetectRamMb(mb_info_ptr);

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
    (void)magic;
    
    SerialSetDebugEnabled(BootDebugRequested(mb_info_ptr));
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  " DISCOUNT_NAME "\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    HalInitialize();
    ShowBootScreen(mb_info_ptr);
    
    ObInit();
    KeInit();
    KeAttachCurrentThread("KernelMain");
    CdfsInit();
    DriverLoadAll(mb_info_ptr);
    KeyboardInit();
    NetInit();
    SubsystemInit(mb_info_ptr);
    SubsystemLaunchSmss();

    for (;;) __asm__ volatile("hlt");
}
