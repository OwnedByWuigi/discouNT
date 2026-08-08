#include <stdint.h>
#include "hal.h"
#include "mm.h"
#include "object.h"
#include "win32k.h"

void TestWndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)hwnd;
    (void)msg;
    (void)wParam;
    (void)lParam;
}

void kmain(uint32_t magic, void *mb_info) {
    (void)magic;
    (void)mb_info;
    
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("NT-like OS v0.2\n", 0x1F);
    HalPutString("================\n", 0x1F);
    HalPutString("HAL initialized\n", 0x0A);
    
    // Object Manager
    ObInit();
    HalPutString("Object Manager initialized\n", 0x0A);
    
    // Win32k
    Win32kInit();
    HalPutString("Window Manager initialized\n", 0x0A);
    
    // Register a test window class
    HalPutString("Registering window class...\n", 0x0F);
    HANDLE hClass = Win32kRegisterClass("TestWindow", 0, TestWndProc);
    if (hClass != INVALID_HANDLE) {
        HalPutString("Window class registered - OK\n", 0x0A);
    }
    
    // Create a test window
    HalPutString("Creating window...\n", 0x0F);
    HANDLE hwnd = Win32kCreateWindow("TestWindow", "Test Window", 
                                      10, 5, 40, 10, 
                                      WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION);
    if (hwnd != INVALID_HANDLE) {
        HalPutString("Window created - OK\n", 0x0A);
        Win32kShowWindow(hwnd);
        HalPutString("Window shown!\n", 0x0A);
    } else {
        HalPutString("FAILED to create window!\n", 0x0C);
    }
    
    while(1) {
        __asm__ volatile("hlt");
    }
}