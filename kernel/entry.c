#include <stdint.h>
#include "hal.h"
#include "object.h"
#include "ke.h"
#include "portio.h"
#include "serial.h"
#include "cdfs.h"
#include "nativecmd.h"
#include "subsystem.h"

void kmain(uint32_t magic, void *mb_info_ptr) {
    (void)magic;
    
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  discouNT\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("discouNT\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Type 'help' for commands\n\n", 0x0F);
    
    ObInit();
    KeInit();
    CdfsInit();
    SubsystemInit(mb_info_ptr);
    NativeCmdInit();
    
    while(1) {
        if (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);
            if (!(status & 0x20) && (status & 1)) NativeCmdHandleScancode(data);
        }
        for (volatile int i = 0; i < 5000; i++);
    }
}
