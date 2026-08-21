#include <stdint.h>
#include "serial.h"
#include "arch/x86/portio.h"

static int serial_ready = 0;

// Check if transmit buffer is empty
static int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void SerialInit(void) {
    // Disable interrupts
    outb(COM1_PORT + 1, 0x00);
    
    // Set baud rate to 115200
    outb(COM1_PORT + 3, 0x80);    // Enable DLAB
    outb(COM1_PORT + 0, 0x01);    // Divisor low byte (115200)
    outb(COM1_PORT + 1, 0x00);    // Divisor high byte
    
    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 3, 0x03);
    
    // Enable FIFO, clear them, 14-byte threshold
    outb(COM1_PORT + 2, 0xC7);
    
    // IRQs enabled, RTS/DSR set
    outb(COM1_PORT + 4, 0x0B);
    
    serial_ready = 1;
    
    // Test output
    SerialPutString("\r\n[Serial] COM1 initialized at 115200 baud\r\n");
}

void SerialSetDebugEnabled(int enabled) {
    (void)enabled;
}

int SerialIsDebugEnabled(void) {
    return 1;
}

void SerialPutChar(char c) {
    if (!serial_ready) return;
    
    // Wait for transmit buffer to be empty
    while (!serial_is_transmit_empty());
    
    // Handle newline
    if (c == '\n') {
        outb(COM1_PORT, '\r');
        while (!serial_is_transmit_empty());
        outb(COM1_PORT, '\n');
    } else {
        outb(COM1_PORT, c);
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
