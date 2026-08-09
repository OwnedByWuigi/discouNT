#include <stdint.h>
#include "hal.h"
#include "mm.h"
#include "object.h"
#include "ke.h"
#include "fb.h"
#include "win32k.h"
#include "mouse.h"
#include "portio.h"
#include "util.h"
#include "serial.h"

static int window_counter = 0;

void TestWndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    if (msg == WM_PAINT) {
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        if (win) {
            int cx = win->x + 10;
            int cy = win->y + 28;
            
            FbDrawString(cx, cy, "This is a window!", COLOR_BLACK, COLOR_LIGHT_GRAY);
            FbDrawString(cx, cy + 16, "Drag title bar to move", COLOR_BLUE, COLOR_LIGHT_GRAY);
            FbDrawString(cx, cy + 32, "Click X to close", COLOR_RED, COLOR_LIGHT_GRAY);
            
            FbFillRect(cx, cy + 55, 80, 30, COLOR_CYAN);
            FbDrawString(cx + 10, cy + 62, "Content", COLOR_BLACK, COLOR_CYAN);
            
            FbFillRect(cx + 90, cy + 55, 80, 30, COLOR_YELLOW);
            FbDrawString(cx + 100, cy + 62, "More", COLOR_BLACK, COLOR_YELLOW);
            
            ObDereferenceObject(hwnd);
        }
    }
    (void)wParam; (void)lParam;
}

static int pt_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static void redraw_buttons(void) {
    int btn_x = 10, btn_y = 10, btn_w = 140, btn_h = 26;
    int btn2_x = 160, btn2_y = 10, btn2_w = 100, btn2_h = 26;
    
    FbFillRect(btn_x, btn_y, btn_w, btn_h, COLOR_DARK_GRAY);
    FbDrawRect(btn_x, btn_y, btn_w, btn_h, COLOR_WHITE);
    FbDrawString(btn_x + 12, btn_y + 6, "Spawn Window", COLOR_WHITE, COLOR_DARK_GRAY);
    
    FbFillRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_DARK_GRAY);
    FbDrawRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_WHITE);
    FbDrawString(btn2_x + 12, btn2_y + 6, "Clear All", COLOR_WHITE, COLOR_DARK_GRAY);
}

static void redraw_status(MOUSE_STATE *ms) {
    FbFillRect(0, 462, 640, 18, COLOR_DARK_GRAY);
    
    char buf[64];
    int p = 0;
    buf[p++] = 'X'; buf[p++] = ':';
    buf[p++] = '0' + (ms->x / 100) % 10;
    buf[p++] = '0' + (ms->x / 10) % 10;
    buf[p++] = '0' + ms->x % 10;
    buf[p++] = ' '; buf[p++] = 'Y'; buf[p++] = ':';
    buf[p++] = '0' + (ms->y / 100) % 10;
    buf[p++] = '0' + (ms->y / 10) % 10;
    buf[p++] = '0' + ms->y % 10;
    buf[p++] = ' '; buf[p++] = 'W'; buf[p++] = ':';
    if (window_counter >= 10) buf[p++] = '0' + (window_counter / 10) % 10;
    buf[p++] = '0' + window_counter % 10;
    buf[p] = 0;
    
    FbDrawString(4, 464, buf, COLOR_WHITE, COLOR_DARK_GRAY);
    FbDrawString(130, 464, "Left click: interact  |  Keys: ignored", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
}

void kmain(uint32_t magic, void *mb_info_ptr) {
    (void)magic;
    
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  NT-like OS v0.8\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("NT-like OS v0.8\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    
    ObInit();
    KeInit();
    Win32kInit(mb_info_ptr);
    MouseInit();
    
    Win32kRegisterClass("TestWindow", 0, TestWndProc);
    
    // Initial screen
    FbClearScreen(COLOR_BLUE);
    redraw_buttons();
    
    FbFillRect(0, 462, 640, 18, COLOR_DARK_GRAY);
    FbDrawString(4, 464, "Click buttons or interact with windows", COLOR_WHITE, COLOR_DARK_GRAY);
    
    MouseDrawCursor();
    FbSwapBuffers();
    
    SerialPutString("[GUI] Ready\r\n\r\n");
    
    MOUSE_STATE ms;
    int last_x = 320, last_y = 240;
    uint8_t last_buttons = 0;
    
    while(1) {
        // Process PS/2 data
        if (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            if (status & 0x20) {
                MouseHandleInterrupt();
            } else {
                inb(0x60);
            }
        }
        
        MouseGetState(&ms);
        
        if (ms.x != last_x || ms.y != last_y || ms.buttons != last_buttons) {
            int need_full_redraw = 0;
            
            // Left button pressed
            if (ms.left_down && !(last_buttons & 1)) {
                // Spawn Window button
                if (pt_in_rect(ms.x, ms.y, 10, 10, 140, 26)) {
                    SerialPutString("[GUI] Spawn window\r\n");
                    
                    char title[12];
                    title[0] = 'W'; title[1] = 'i'; title[2] = 'n'; title[3] = 'd';
                    title[4] = 'o'; title[5] = 'w'; title[6] = ' ';
                    int n = window_counter;
                    title[7] = (n >= 10) ? '0' + (n / 10) % 10 : ' ';
                    title[8] = '0' + n % 10;
                    title[9] = 0;
                    
                    int wx = 50 + (n * 30) % 320;
                    int wy = 60 + (n * 25) % 220;
                    
                    HANDLE hwnd = Win32kCreateWindow("TestWindow", title, wx, wy, 280, 180,
                                                      WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION);
                    Win32kShowWindow(hwnd);
                    Win32kUpdateWindow(hwnd);
                    window_counter++;
                    need_full_redraw = 1;
                }
                // Clear All button
                else if (pt_in_rect(ms.x, ms.y, 160, 10, 100, 26)) {
                    SerialPutString("[GUI] Clear all\r\n");
                    FbClearScreen(COLOR_BLUE);
                    window_counter = 0;
                    need_full_redraw = 1;
                }
                // Window interaction
                else {
                    Win32kHandleMouseDown(ms.x, ms.y, 1);
                }
            }
            
            // Left button released
            if (!ms.left_down && (last_buttons & 1)) {
                Win32kHandleMouseUp(ms.x, ms.y, 1);
            }
            
            // Dragging
            if (ms.left_down) {
                Win32kHandleMouseMove(ms.x, ms.y);
            }
            
            // Erase cursor before any redrawing
            MouseEraseCursor();
            
            if (need_full_redraw) {
                Win32kRedrawAll();
            }
            
            // Always redraw buttons and status
            redraw_buttons();
            redraw_status(&ms);
            
            // Draw cursor last
            MouseDrawCursor();
            FbSwapBuffers();
            
            last_x = ms.x;
            last_y = ms.y;
            last_buttons = ms.buttons;
        }
        
        for (volatile int i = 0; i < 500; i++);
    }
}