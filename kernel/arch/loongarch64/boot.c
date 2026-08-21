#include <stdint.h>
#include "core/util.h"
#include "core/ke.h"
#include "core/subsystem.h"
#include "hal.h"
#include "io/io.h"
#include "mm/mm.h"
#include "ob/object.h"
#include "serial.h"
#include "smss.h"
#include "csrss.h"
#include "fb.h"
#include "cdfs.h"
#include "usb.h"
#include "keyboard.h"

/* QEMU LoongArch virt machine's 16550-compatible debug UART. */
#define UART_BASE 0x1fe001e0UL
#define UART_THR  0
#define UART_LSR  5
#define UART_LSR_THRE 0x20

extern uint8_t __bss_start;
extern uint8_t __bss_end;
extern uint8_t __data_start;
extern uint8_t __data_end;
extern uint8_t __data_load;

uint32_t KeGetProcessorCount(void) { return 1; }
uint32_t KeGetPhysicalMemoryPages(void) { return (512U * 1024U * 1024U) / 4096U; }

static void uart_putc(char c) {
    volatile uint8_t *uart = (volatile uint8_t *)UART_BASE;
    while ((uart[UART_LSR] & UART_LSR_THRE) == 0) {
    }
    uart[UART_THR] = (uint8_t)c;
}

static void uart_puts(const char *text) {
    while (*text) {
        if (*text == '\n') uart_putc('\r');
        uart_putc(*text++);
    }
}

/*
 * QEMU's firmware-assisted ELF loader enters here.  The incoming registers
 * follow the LoongArch boot protocol and will be captured in the next stage.
 */
__attribute__((noreturn)) void boot_main(void) {
    uint8_t *bss;
    uint8_t *data;
    uint8_t *data_load;
    IO_DRIVER_OBJECT *driver;
    IO_DEVICE_OBJECT *device;

    data_load = &__data_load;
    for (data = &__data_start; data < &__data_end; ++data)
        *data = *data_load++;
    for (bss = &__bss_start; bss < &__bss_end; ++bss) *bss = 0;
    uart_puts("discouNT LoongArch64 port\n");
    uart_puts("early kernel entry reached successfully\n");

    SerialInit();
    HalInitialize();
    HalClearScreen(0x1f);
    HalPutString("discouNT LoongArch64\n", 0x1f);
    HalPutString("Native display console initialized\n\n", 0x0f);
    MmInitialize(0);
    ObInit();
    IoInit();
    KeInit();
    KeAttachCurrentThread("KernelMain");
    SerialPutString("[LA64] Runtime, memory, object, I/O, and executive initialized\n");

    driver = IoCreateDriver("La64Platform", 0, 0);
    device = driver ? IoCreateDevice(driver, "La64Uart0", 0) : 0;
    if (device)
        SerialPutString("[LA64] Platform device registered with shared I/O manager\n");
    else
        SerialPutString("[LA64] Platform device registration failed\n");

    UsbBootInitialize();
    CdfsInit();
    KeyboardInit();
    SubsystemInit(0);
    if (SmssIsInitialized())
        HalPutString("SMSS SESSION MANAGER INITIALIZED\n", 0x0a);
    SubsystemLaunchSmss();
    if (CsrssIsInitialized())
        HalPutString("CSRSS RUNTIME PROCESS INITIALIZED\n", 0x0a);

    for (;;) {
        __asm__ volatile("idle 0");
    }
}
