#include <stdint.h>
#include "serial.h"
#if defined(__loongarch64)
#define LA64_UART_BASE 0x1fe001e0UL
static volatile uint8_t *SerialUart(void) {
    return (volatile uint8_t *)LA64_UART_BASE;
}
static uint8_t SerialReadRegister(uint32_t offset) {
    return SerialUart()[offset];
}
static void SerialWriteRegister(uint32_t offset, uint8_t value) {
    SerialUart()[offset] = value;
}
#else
#include "arch/x86/portio.h"
static uint8_t SerialReadRegister(uint32_t offset) {
    return inb(COM1_PORT + (uint16_t)offset);
}
static void SerialWriteRegister(uint32_t offset, uint8_t value) {
    outb(COM1_PORT + (uint16_t)offset, value);
}
#endif

static int serial_ready = 0;
static int serial_debug_enabled = 1;
static int serial_screen_debug_enabled;

// Check if transmit buffer is empty
static int serial_is_transmit_empty(void) {
    return SerialReadRegister(5) & 0x20;
}

void SerialInit(void) {
#if defined(__loongarch64)
    /* QEMU firmware/reset code configures its 16550-compatible UART. */
    serial_ready = 1;
#else
    // Disable interrupts
    SerialWriteRegister(1, 0x00);
    
    // Set baud rate to 115200
    SerialWriteRegister(3, 0x80); // Enable DLAB
    SerialWriteRegister(0, 0x01); // Divisor low byte (115200)
    SerialWriteRegister(1, 0x00); // Divisor high byte
    
    // 8 bits, no parity, one stop bit
    SerialWriteRegister(3, 0x03);
    
    // Enable FIFO, clear them, 14-byte threshold
    SerialWriteRegister(2, 0xC7);
    
    // IRQs enabled, RTS/DSR set
    SerialWriteRegister(4, 0x0B);
    
    serial_ready = 1;
#endif
    
    // Test output
    SerialPutString("\r\n[Serial] COM1 initialized at 115200 baud\r\n");
}

void SerialSetDebugEnabled(int enabled) {
    serial_debug_enabled = enabled != 0;
}

int SerialIsDebugEnabled(void) {
    return serial_debug_enabled;
}

void SerialSetScreenDebugEnabled(int enabled) {
    serial_screen_debug_enabled = enabled != 0;
}

int SerialIsScreenDebugEnabled(void) {
    return serial_screen_debug_enabled;
}

void SerialPutChar(char c) {
    if (!serial_ready) return;
    
    // Wait for transmit buffer to be empty
    while (!serial_is_transmit_empty());
    
    // Handle newline
    if (c == '\n') {
        SerialWriteRegister(0, '\r');
        while (!serial_is_transmit_empty());
        SerialWriteRegister(0, '\n');
    } else {
        SerialWriteRegister(0, (uint8_t)c);
    }
}

void SerialPutString(const char *str) {
    while (*str) {
        SerialPutChar(*str++);
    }
}

void SerialPrintHex(uint32_t val) {
    char buf[16];
    char *p = buf;
    
    *p++ = '0';
    *p++ = 'x';
    
    if (val == 0) {
        *p++ = '0';
    } else {
        int leading = 1;
        for (int i = 28; i >= 0; i -= 4) {
            uint8_t nibble = (val >> i) & 0xF;
            if (nibble != 0 || !leading || i == 0) {
                *p++ = "0123456789ABCDEF"[nibble];
                leading = 0;
            }
        }
    }
    *p = 0;
    
    SerialPutString(buf);
}

void SerialPrintDec(uint32_t val) {
    char buf[16];
    char *p = buf + 15;
    *p = 0;
    
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            *--p = '0' + (val % 10);
            val /= 10;
        }
    }
    
    SerialPutString(p);
}
