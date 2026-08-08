#ifndef VGA_H
#define VGA_H
#include <stdint.h>

#define VGA_WIDTH  640
#define VGA_HEIGHT 480

// Colors
#define COLOR_BLACK        0
#define COLOR_BLUE         1
#define COLOR_GREEN        2
#define COLOR_CYAN         3
#define COLOR_RED          4
#define COLOR_MAGENTA      5
#define COLOR_BROWN        6
#define COLOR_LIGHT_GRAY   7
#define COLOR_DARK_GRAY    8
#define COLOR_LIGHT_BLUE   9
#define COLOR_LIGHT_GREEN  10
#define COLOR_LIGHT_CYAN   11
#define COLOR_LIGHT_RED    12
#define COLOR_LIGHT_MAGENTA 13
#define COLOR_YELLOW       14
#define COLOR_WHITE        15

void VgaInit(void);
void VgaClearScreen(uint8_t color);
void VgaPutPixel(int x, int y, uint8_t color);
void VgaFillRect(int x, int y, int w, int h, uint8_t color);
void VgaDrawRect(int x, int y, int w, int h, uint8_t color);
void VgaDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg);
void VgaDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg);
void VgaSwapBuffers(void);
// Add at the end of vga.h, before #endif
extern uint8_t back_buffer[];
#endif