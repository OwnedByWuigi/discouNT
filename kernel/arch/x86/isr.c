#include <stdint.h>
#include "serial.h"
#include "core/bugcheck.h"

void exception_handler_c(uint32_t vector, uint32_t error_code, uint32_t eip) {
    KeBugCheckEx(0x0000007EU, vector, error_code, eip, 0);
}
