// kernel/hal.c
#include <stdint.h>
#include "hal.h"
#include "portio.h"

#define VGA_ADDR ((uint16_t*)0xB8000)
static uint16_t cursor = 0;

void HalInitialize(void) {
    // clear screen (black background, light grey text)
    for (int i = 0; i < 80*25; i++)
        VGA_ADDR[i] = 0x0F20;   // space with attribute 0x0F
    cursor = 0;
}

void HalDisplayString(const char *str) {
    while (*str) {
        if (*str == '\n') {
            cursor = (cursor / 80 + 1) * 80;
        } else {
            VGA_ADDR[cursor++] = 0x0F00 | *str;
        }
        if (cursor >= 80*25) cursor = 0; // wrap
        str++;
    }
}