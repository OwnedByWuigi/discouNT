#include <stdint.h>
#include "smss.h"
#include "csrss.h"
#include "serial.h"
#include "hal.h"
#include "cdfs.h"
#include "mm/mm.h"
#include "loader/peloader.h"

#define CSRSS_RETURN_MAGIC 0x43535253

static int smss_execute_bootstrap(const char *path) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;
    int ret;

    if (!CdfsReadFile(path, &file_buf, &file_size)) {
        SerialPutString("[SMSS] Missing bootstrap image: ");
        SerialPutString(path);
        SerialPutString("\r\n");
        return -1;
    }

    image = PeLoadImage(file_buf, file_size);
    if (!image) {
        SerialPutString("[SMSS] Failed to map bootstrap image\r\n");
        kfree(file_buf);
        return -2;
    }

    if (!PeResolveImports(image)) {
        SerialPutString("[SMSS] Import resolution failed: ");
        if (PeGetLastError()) SerialPutString(PeGetLastError());
        SerialPutString("\r\n");
        PeFreeImage(image);
        kfree(file_buf);
        return -4;
    }
    if (file_size >= 2 && file_buf[0] == 0x4D && file_buf[1] == 0x5A) {
        PePerformRelocations(image);
    }

    {
        uint8_t *exe_stack = (uint8_t*)kmalloc(65536);
        uint32_t exe_esp;
        uint32_t saved_esp;
        typedef int (*EntryFunc)(void);
        EntryFunc func = (EntryFunc)PeGetEntryPoint(image);

        if (!func || !exe_stack) {
            if (exe_stack) kfree(exe_stack);
            PeFreeImage(image);
            kfree(file_buf);
            return -5;
        }

        exe_esp = (uint32_t)(exe_stack + 65536 - 256);
        __asm__ volatile(
            "movl %%esp, %[oldsp]\n"
            "movl %[newsp], %%esp\n"
            "call *%[fn]\n"
            "movl %%eax, %[retval]\n"
            "movl %[oldsp], %%esp\n"
            :
              [oldsp] "=&r"(saved_esp),
              [retval] "=r"(ret)
            : [newsp] "r"(exe_esp),
              [fn] "r"(func)
            : "eax", "ecx", "edx", "memory"
        );
        kfree(exe_stack);
    }

    PeFreeImage(image);
    kfree(file_buf);
    return ret;
}

void SmssSessionRun(void *mb_info) {
    int ret;

    if (!SmssInitialize()) return;

    SerialPutString("[SMSS] Starting Session Manager Subsystem\r\n");
    ret = smss_execute_bootstrap("/SYSTEM32/CSRSS.EXE");
    if (ret != CSRSS_RETURN_MAGIC && ret != 0) {
        SerialPutString("[SMSS] CSRSS bootstrap failed, status=");
        SerialPrintHex((uint32_t)ret);
        SerialPutString("\r\n");
        if (!SerialIsScreenDebugEnabled()) {
            HalInitialize();
            HalClearScreen(0x1F);
            HalPutString("Failed to launch CSRSS.EXE from SYSTEM32.\n", 0x0C);
        }
        return;
    }

    SerialPutString("[SMSS] Handing off session to CSRSS\r\n");
    CsrssSessionRun(mb_info);
}
