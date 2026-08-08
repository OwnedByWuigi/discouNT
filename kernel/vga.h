#ifndef VGA_H
#define VGA_H
#include <stdint.h>

#define VGA_WIDTH  640
#define VGA_HEIGHT 480

// VGA ports
#define VGA_AC_INDEX    0x3C0
#define VGA_AC_WRITE    0x3C0
#define VGA_AC_READ     0x3C1
#define VGA_MISC_WRITE  0x3C2
#define VGA_SEQ_INDEX   0x3C4
#define VGA_SEQ_DATA    0x3C5
#define VGA_DAC_READ_INDEX  0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA    0x3C9
#define VGA_MISC_READ   0x3CC
#define VGA_GC_INDEX    0x3CE
#define VGA_GC_DATA     0x3CF
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define VGA_INSTAT_READ 0x3DA

// Standard 16 colors
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
void VgaDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg);
void VgaDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg);
void VgaDrawRect(int x, int y, int w, int h, uint8_t color);
void VgaFillRect(int x, int y, int w, int h, uint8_t color);
void VgaDrawLine(int x1, int y1, int x2, int y2, uint8_t color);
void VgaSwapBuffers(void);
#endif