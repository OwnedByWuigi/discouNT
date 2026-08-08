#include <stdint.h>
#include "hal.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_attr = 0x1F; // White on blue

void HalInitialize(void) {
    HalClearScreen(0x1F);
}

void HalClearScreen(uint8_t color) {
    uint16_t blank = 0x20 | (color << 8);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    current_attr = color;
}

void HalPutChar(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else {
        int index = cursor_y * VGA_WIDTH + cursor_x;
        VGA_MEMORY[index] = (uint16_t)c | ((uint16_t)color << 8);
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        // Scroll up
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[(y-1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
            }
        }
        // Clear bottom line
        uint16_t blank = 0x20 | ((uint16_t)color << 8);
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(VGA_HEIGHT-1) * VGA_WIDTH + x] = blank;
        }
        cursor_y = VGA_HEIGHT - 1;
    }
}

void HalPutString(const char *str, uint8_t color) {
    while (*str) {
        HalPutChar(*str++, color);
    }
}

void HalSetCursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

void HalGetCursor(int *x, int *y) {
    *x = cursor_x;
    *y = cursor_y;
}

uint8_t HalGetAttribute(void) {
    return current_attr;
}