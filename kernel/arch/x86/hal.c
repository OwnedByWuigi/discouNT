#include <stdint.h>
#include "arch/x86/hal.h"
#include "serial.h"
#include "arch/x86/multiboot.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_attr = 0x1F;
static uint8_t *framebuffer;
static uint32_t framebuffer_pitch;
static uint32_t framebuffer_width;
static uint32_t framebuffer_height;
static uint8_t framebuffer_bytes;
static uint8_t red_position, green_position, blue_position;

/* Compact 5x7 boot font. Lowercase is deliberately shown as uppercase: this
 * console only has to remain useful until the framebuffer driver is loaded. */
static const uint8_t boot_font[][7] = {
    {0,0,0,0,0,0,0}, {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2},
    {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,2,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

void HalConfigureBootDisplay(void *multiboot_info) {
    MULTIBOOT_INFO *mbi = (MULTIBOOT_INFO*)multiboot_info;
    framebuffer = 0;
    if (!mbi || !(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) ||
        mbi->framebuffer_type != 1 || mbi->framebuffer_bpp < 15 ||
        mbi->framebuffer_bpp > 32 || mbi->framebuffer_addr > 0xFFFFFFFFULL)
        return;
    framebuffer = (uint8_t*)(uintptr_t)mbi->framebuffer_addr;
    framebuffer_pitch = mbi->framebuffer_pitch;
    framebuffer_width = mbi->framebuffer_width;
    framebuffer_height = mbi->framebuffer_height;
    framebuffer_bytes = (mbi->framebuffer_bpp + 7) / 8;
    red_position = mbi->color_info.rgb.red_field_position;
    green_position = mbi->color_info.rgb.green_field_position;
    blue_position = mbi->color_info.rgb.blue_field_position;
}

static uint32_t HalRgb(uint8_t color) {
    static const uint8_t levels[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
        {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    return ((uint32_t)levels[color & 15][0] << red_position) |
           ((uint32_t)levels[color & 15][1] << green_position) |
           ((uint32_t)levels[color & 15][2] << blue_position);
}

static void HalPixel(uint32_t x, uint32_t y, uint32_t value) {
    uint8_t *p;
    if (!framebuffer || x >= framebuffer_width || y >= framebuffer_height) return;
    p = framebuffer + y * framebuffer_pitch + x * framebuffer_bytes;
    for (uint8_t i = 0; i < framebuffer_bytes; i++) p[i] = (uint8_t)(value >> (i * 8));
}

static const uint8_t *HalGlyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c == ' ') return boot_font[0];
    if (c >= '0' && c <= '9') return boot_font[1 + c - '0'];
    if (c >= 'A' && c <= 'Z') return boot_font[11 + c - 'A'];
    return boot_font[0];
}

void HalInitialize(void) {
    // Initialize serial first for debugging
    SerialInit();
    
    cursor_x = 0;
    cursor_y = 0;
    current_attr = 0x1F;
    
    SerialPutString("[HAL] Text mode initialized\r\n");
}

void HalClearScreen(uint8_t color) {
    uint16_t blank = 0x20 | ((uint16_t)color << 8);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    if (framebuffer) {
        uint32_t rgb = HalRgb(color & 15);
        for (uint32_t y = 0; y < framebuffer_height; y++)
            for (uint32_t x = 0; x < framebuffer_width; x++) HalPixel(x, y, rgb);
    }
    cursor_x = 0;
    cursor_y = 0;
    current_attr = color;
}

void HalPutChar(char c, uint8_t color) {
    // Output to serial
    SerialPutChar(c);
    
    // Output to VGA text mode
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        int index = cursor_y * VGA_WIDTH + cursor_x;
        VGA_MEMORY[index] = ((uint16_t)c) | ((uint16_t)color << 8);
        if (framebuffer) {
            const uint8_t *glyph = HalGlyph(c);
            uint32_t fg = HalRgb(color & 15), bg = HalRgb(color >> 4);
            for (uint32_t gy = 0; gy < 8; gy++)
                for (uint32_t gx = 0; gx < 8; gx++) {
                    int set = gy < 7 && gx < 5 && (glyph[gy] & (1U << (4 - gx)));
                    HalPixel((uint32_t)cursor_x * 8 + gx, (uint32_t)cursor_y * 8 + gy, set ? fg : bg);
                }
        }
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        // Scroll
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[(y-1)*VGA_WIDTH + x] = VGA_MEMORY[y*VGA_WIDTH + x];
            }
        }
        uint16_t blank = 0x20 | ((uint16_t)color << 8);
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(VGA_HEIGHT-1)*VGA_WIDTH + x] = blank;
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
