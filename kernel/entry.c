#include <stdint.h>
#include "hal.h"
#include "vga.h"

void kmain(uint32_t magic, void *mb_info) {
    (void)magic;
    (void)mb_info;
    
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("Switching to VGA mode...\n", 0x0F);
    
    VgaInit();
    VgaClearScreen(COLOR_BLUE);
    
    // Title bar
    VgaFillRect(0, 0, VGA_WIDTH, 24, COLOR_WHITE);
    VgaDrawString(8, 4, "NT-like OS v0.3 - VGA 640x480x16", COLOR_BLACK, COLOR_WHITE);
    
    // Color test blocks
    int colors[] = {COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
                    COLOR_RED, COLOR_MAGENTA, COLOR_BROWN, COLOR_LIGHT_GRAY,
                    COLOR_DARK_GRAY, COLOR_LIGHT_BLUE, COLOR_LIGHT_GREEN, COLOR_LIGHT_CYAN,
                    COLOR_LIGHT_RED, COLOR_LIGHT_MAGENTA, COLOR_YELLOW, COLOR_WHITE};
    
    for (int i = 0; i < 16; i++) {
        VgaFillRect(8 + i * 38, 32, 34, 24, colors[i]);
    }
    
    // Window
    VgaFillRect(40, 70, 560, 380, COLOR_LIGHT_GRAY);
    VgaDrawRect(40, 70, 560, 380, COLOR_BLACK);
    
    // Window title bar
    VgaFillRect(40, 70, 560, 20, COLOR_DARK_GRAY);
    VgaDrawString(48, 73, "Test Window", COLOR_WHITE, COLOR_DARK_GRAY);
    
    // Close button
    VgaFillRect(576, 72, 18, 16, COLOR_RED);
    VgaDrawString(581, 74, "X", COLOR_WHITE, COLOR_RED);
    
    // Window content
    VgaDrawString(56, 100, "VGA Graphics Mode Working!", COLOR_BLACK, COLOR_LIGHT_GRAY);
    VgaDrawString(56, 120, "Resolution: 640 x 480 x 16 colors", COLOR_BLUE, COLOR_LIGHT_GRAY);
    VgaDrawString(56, 145, "Features:", COLOR_RED, COLOR_LIGHT_GRAY);
    VgaDrawString(72, 165, "True 16-color VGA output", COLOR_BLACK, COLOR_LIGHT_GRAY);
    VgaDrawString(72, 185, "8x8 bitmap font rendering", COLOR_BLACK, COLOR_LIGHT_GRAY);
    VgaDrawString(72, 205, "Rectangle fill and outline", COLOR_BLACK, COLOR_LIGHT_GRAY);
    VgaDrawString(72, 225, "Line drawing", COLOR_BLACK, COLOR_LIGHT_GRAY);
    VgaDrawString(72, 245, "Direct VGA hardware access", COLOR_BLACK, COLOR_LIGHT_GRAY);
    
    // Draw some demo shapes
    VgaFillRect(400, 100, 80, 50, COLOR_GREEN);
    VgaDrawString(415, 118, "GREEN", COLOR_BLACK, COLOR_GREEN);
    
    VgaFillRect(400, 160, 80, 50, COLOR_CYAN);
    VgaDrawString(420, 178, "CYAN", COLOR_BLACK, COLOR_CYAN);
    
    VgaFillRect(400, 220, 80, 50, COLOR_YELLOW);
    VgaDrawString(410, 238, "YELLOW", COLOR_BLACK, COLOR_YELLOW);
    
    VgaDrawRect(395, 95, 90, 180, COLOR_BLACK);
    
    // Lines
    VgaDrawLine(56, 280, 300, 280, COLOR_RED);
    VgaDrawLine(56, 285, 300, 285, COLOR_GREEN);
    VgaDrawLine(56, 290, 300, 290, COLOR_BLUE);
    
    VgaDrawString(56, 310, "Line drawing demo:", COLOR_BLACK, COLOR_LIGHT_GRAY);
    
    // Triangle
    VgaDrawLine(56, 350, 200, 420, COLOR_BLACK);
    VgaDrawLine(200, 420, 344, 350, COLOR_BLACK);
    VgaDrawLine(344, 350, 56, 350, COLOR_BLACK);
    VgaDrawString(130, 425, "Triangle", COLOR_BLACK, COLOR_LIGHT_GRAY);
    
    // Status bar
    VgaFillRect(0, VGA_HEIGHT - 20, VGA_WIDTH, 20, COLOR_WHITE);
    VgaDrawString(8, VGA_HEIGHT - 16, "System Ready - All tests passed!", COLOR_BLACK, COLOR_WHITE);
    
    while(1) {
        __asm__ volatile("hlt");
    }
}