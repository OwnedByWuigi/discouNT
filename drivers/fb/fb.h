#ifndef FB_H
#define FB_H
#include <stdint.h>

extern int fb_width;
extern int fb_height;

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

void FbInit(void *multiboot_info);
void FbClearScreen(uint8_t color);
void FbPutPixel(int x, int y, uint8_t color);
void FbFillRect(int x, int y, int w, int h, uint8_t color);
void FbDrawRect(int x, int y, int w, int h, uint8_t color);
void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg);
void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg);
void FbSwapBuffers(void);
int FbIsFramebuffer(void);
int FbGetWidth(void);
int FbGetHeight(void);
int FbGetModeCount(void);
int FbGetModeInfo(int index, int *width, int *height, int *bpp);
int FbSetResolution(int width, int height, int bpp);
uint8_t FbGetPixel(int x, int y);
void FbCapture(uint8_t *dst, int dst_stride);
void FbBlitIndexed(int x, int y, int w, int h, const uint8_t *src, int src_stride);

#endif
