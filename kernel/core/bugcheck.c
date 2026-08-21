#include <stdint.h>
#include "core/bugcheck.h"
#include "hal.h"
#include "fb.h"
#include "serial.h"
#include "cpu.h"

static void bugcheck_hex(char *out, uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int i;
    out[0] = '0'; out[1] = 'x';
    for (i = 0; i < 8; i++) out[2 + i] = digits[(value >> (28 - i * 4)) & 0xF];
    out[10] = 0;
}

void KeBugCheckEx(uint32_t code, uint32_t p1, uint32_t p2,
                  uint32_t p3, uint32_t p4) {
    static volatile int entered = 0;
    char code_text[11], p1_text[11], p2_text[11], p3_text[11], p4_text[11];
    if (entered) CpuHalt();
    entered = 1;
    CpuDisableInterrupts();

    bugcheck_hex(code_text, code); bugcheck_hex(p1_text, p1);
    bugcheck_hex(p2_text, p2); bugcheck_hex(p3_text, p3); bugcheck_hex(p4_text, p4);
    SerialPutString("[BUGCHECK] code="); SerialPutString(code_text);
    SerialPutString(" p1="); SerialPutString(p1_text);
    SerialPutString(" p2="); SerialPutString(p2_text);
    SerialPutString(" p3="); SerialPutString(p3_text);
    SerialPutString(" p4="); SerialPutString(p4_text); SerialPutString("\r\n");

    if (FbIsFramebuffer()) {
        int w = FbGetWidth(), h = FbGetHeight();
        FbFillRect(0, 0, w, h, COLOR_BLUE);
        FbDrawString(24, 28, "discouNT has encountered a problem and has stopped.", COLOR_WHITE, COLOR_BLUE);
        FbDrawString(24, 52, "A bugcheck was issued to prevent further system corruption.", COLOR_WHITE, COLOR_BLUE);
        FbDrawString(24, 92, "STOP:", COLOR_WHITE, COLOR_BLUE); FbDrawString(80, 92, code_text, COLOR_WHITE, COLOR_BLUE);
        FbDrawString(24, 120, "Parameters:", COLOR_WHITE, COLOR_BLUE); FbDrawString(120, 120, p1_text, COLOR_WHITE, COLOR_BLUE);
        FbDrawString(120, 140, p2_text, COLOR_WHITE, COLOR_BLUE); FbDrawString(120, 160, p3_text, COLOR_WHITE, COLOR_BLUE);
        FbDrawString(120, 180, p4_text, COLOR_WHITE, COLOR_BLUE);
        FbSwapBuffers();
    } else {
        HalInitialize();
        HalClearScreen(0x1F);
        HalPutString("\n\ndiscouNT has encountered a problem and has stopped.\n\n", 0x1F);
        HalPutString("STOP: ", 0x1F); HalPutString(code_text, 0x1F); HalPutString("\n", 0x1F);
        HalPutString("Parameters: ", 0x1F); HalPutString(p1_text, 0x1F); HalPutString(" ", 0x1F);
        HalPutString(p2_text, 0x1F); HalPutString(" ", 0x1F); HalPutString(p3_text, 0x1F);
        HalPutString(" ", 0x1F); HalPutString(p4_text, 0x1F); HalPutString("\n\n", 0x1F);
        HalPutString("The system has been halted to prevent damage.\n", 0x1F);
    }
    CpuHalt();
}

void KeBugCheck(uint32_t code) { KeBugCheckEx(code, 0, 0, 0, 0); }
