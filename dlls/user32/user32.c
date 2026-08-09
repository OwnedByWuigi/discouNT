// user32.c - User interface API
#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *h, uint32_t r, void *l) {
    (void)h; (void)r; (void)l; return 1;
}

__attribute__((stdcall)) int MessageBoxA(void *hwnd, const char *text, 
                                           const char *caption, uint32_t type) {
    (void)hwnd; (void)type;
    extern void VgaDrawString(int x, int y, const char *s, uint8_t fg, uint8_t bg);
    extern void VgaFillRect(int x, int y, int w, int h, uint8_t c);
    extern void VgaDrawRect(int x, int y, int w, int h, uint8_t c);
    extern void VgaSwapBuffers(void);
    
    // Draw a simple message box
    VgaFillRect(100, 100, 440, 200, 7);  // Light gray
    VgaDrawRect(100, 100, 440, 200, 0);   // Black border
    VgaFillRect(100, 100, 440, 20, 8);    // Dark gray title
    VgaDrawString(108, 103, caption ? caption : "Message", 15, 8);
    VgaDrawString(120, 140, text ? text : "", 0, 7);
    
    // OK button
    VgaFillRect(280, 260, 60, 22, 8);
    VgaDrawRect(280, 260, 60, 22, 0);
    VgaDrawString(298, 263, "OK", 15, 8);
    VgaSwapBuffers();
    
    return 1;
}

__attribute__((stdcall)) int ShowWindow(void *hwnd, int cmd) {
    (void)hwnd; (void)cmd;
    return 1;
}

__attribute__((stdcall)) int UpdateWindow(void *hwnd) {
    (void)hwnd;
    return 1;
}

__attribute__((stdcall)) int GetMessageA(void *msg, void *hwnd, uint32_t min, uint32_t max) {
    (void)msg; (void)hwnd; (void)min; (void)max;
    return 0; // Return 0 = WM_QUIT
}

__attribute__((stdcall)) int DispatchMessageA(void *msg) {
    (void)msg;
    return 0;
}

__attribute__((stdcall)) void PostQuitMessage(int code) {
    (void)code;
}

__attribute__((stdcall)) uint32_t DefWindowProcA(void *h, uint32_t m, uint32_t w, uint32_t l) {
    (void)h; (void)m; (void)w; (void)l;
    return 0;
}