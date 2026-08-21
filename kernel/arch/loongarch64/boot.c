#include <stdint.h>

/* QEMU LoongArch virt machine's 16550-compatible debug UART. */
#define UART_BASE 0x1fe001e0UL
#define UART_THR  0
#define UART_LSR  5
#define UART_LSR_THRE 0x20

static volatile uint8_t *const uart = (volatile uint8_t *)UART_BASE;

static void uart_putc(char c) {
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
    uart_puts("discouNT LoongArch64 port\n");
    uart_puts("early kernel entry reached successfully\n");

    for (;;) {
        __asm__ volatile("idle 0");
    }
}
