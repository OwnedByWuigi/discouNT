// gdi32.c - Graphics Device Interface
#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *h, uint32_t r, void *l) {
    (void)h; (void)r; (void)l; return 1;
}

__attribute__((stdcall)) int TextOutA(void *dc, int x, int y, const char *str, int len) {
    (void)dc;
    extern void VgaDrawString(int x, int y, const char *s, uint8_t fg, uint8_t bg);
    if (len == 0) {
        extern uint32_t strlen(const char *s);
        len = strlen(str);
    }
    // Create a temporary null-terminated copy
    char buf[256];
    int i;
    for (i = 0; i < len && i < 255; i++) buf[i] = str[i];
    buf[i] = 0;
    VgaDrawString(x, y, buf, 0, 7);
    return 1;
}

__attribute__((stdcall)) void *GetStockObject(int obj) {
    (void)obj;
    return (void*)1;
}

__attribute__((stdcall)) int SetBkMode(void *dc, int mode) {
    (void)dc; (void)mode;
    return 1;
}

__attribute__((stdcall)) uint32_t SetTextColor(void *dc, uint32_t color) {
    (void)dc; (void)color;
    return 0;
}

__attribute__((stdcall)) int Rectangle(void *dc, int l, int t, int r, int b) {
    (void)dc;
    extern void VgaDrawRect(int x, int y, int w, int h, uint8_t c);
    VgaDrawRect(l, t, r - l, b - t, 0);
    return 1;
}