#include <stdint.h>
#include "serial.h"

// Exception handler that just returns
// This prevents triple faults from crashing the system
__attribute__((naked)) void exception_handler(void) {
    __asm__ volatile(
        "pusha\n"
        "call exception_handler_c\n"
        "popa\n"
        "add $4, %esp\n"  // Remove error code
        "iret\n"
    );
}

void exception_handler_c(uint32_t error_code) {
    SerialPutString("[EXCEPTION] Caught exception: ");
    SerialPrintHex(error_code);
    SerialPutString("\r\n");
    // Don't halt - just return
}