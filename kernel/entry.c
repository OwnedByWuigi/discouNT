#include <stdint.h>
#include "hal.h"
#include "mm.h"
#include "object.h"
#include "ke.h"
#include "vga.h"
#include "win32k.h"
#include "mouse.h"
#include "portio.h"
#include "util.h"
#include "serial.h"

// Global window counter
static int window_counter = 0;

// Window procedure for test windows
void TestWndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    if (msg == WM_PAINT) {
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        if (win) {
            int cx = win->x + 10;
            int cy = win->y + 28;
            
            VgaDrawString(cx, cy, "This is a window!", COLOR_BLACK, COLOR_LIGHT_GRAY);
            VgaDrawString(cx, cy + 16, "Drag title bar to move", COLOR_BLUE, COLOR_LIGHT_GRAY);
            VgaDrawString(cx, cy + 32, "Click X to close", COLOR_RED, COLOR_LIGHT_GRAY);
            
            // Draw some colored content
            VgaFillRect(cx, cy + 55, 80, 30, COLOR_CYAN);
            VgaDrawString(cx + 10, cy + 62, "Content", COLOR_BLACK, COLOR_CYAN);
            
            VgaFillRect(cx + 90, cy + 55, 80, 30, COLOR_YELLOW);
            VgaDrawString(cx + 100, cy + 62, "More", COLOR_BLACK, COLOR_YELLOW);
            
            ObDereferenceObject(hwnd);
        }
    }
    (void)wParam; (void)lParam;
}

// Check if a point is inside a rectangle
static int pt_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

void kmain(uint32_t magic, void *mb_info_ptr) {
    (void)magic;
    (void)mb_info_ptr;
    
    // Initialize serial debug
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  NT-like OS v0.8 - Win32k GUI\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    // Initialize subsystems
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("NT-like OS v0.8\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    
    SerialPutString("[INIT] Object Manager...\r\n");
    ObInit();
    
    SerialPutString("[INIT] Executive...\r\n");
    KeInit();
    
    SerialPutString("[INIT] VGA Graphics...\r\n");
    Win32kInit();
    
    SerialPutString("[INIT] PS/2 Mouse...\r\n");
    MouseInit();
    
    // Register window class
    Win32kRegisterClass("TestWindow", 0, TestWndProc);
    
    // Draw blank desktop
    VgaClearScreen(COLOR_BLUE);
    
    // Button dimensions
    int btn_x = 10, btn_y = 10, btn_w = 140, btn_h = 26;
    
    // Draw "Spawn Window" button
    VgaFillRect(btn_x, btn_y, btn_w, btn_h, COLOR_DARK_GRAY);
    VgaDrawRect(btn_x, btn_y, btn_w, btn_h, COLOR_WHITE);
    VgaDrawString(btn_x + 12, btn_y + 6, "Spawn Window", COLOR_WHITE, COLOR_DARK_GRAY);
    
    // Draw "Clear All" button
    int btn2_x = 160, btn2_y = 10, btn2_w = 100, btn2_h = 26;
    VgaFillRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_DARK_GRAY);
    VgaDrawRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_WHITE);
    VgaDrawString(btn2_x + 12, btn2_y + 6, "Clear All", COLOR_WHITE, COLOR_DARK_GRAY);
    
    // Status bar
    VgaFillRect(0, 462, 640, 18, COLOR_DARK_GRAY);
    VgaDrawString(4, 464, "Click buttons or interact with windows", COLOR_WHITE, COLOR_DARK_GRAY);
    
    // Draw mouse cursor and flush
    MouseDrawCursor();
    VgaSwapBuffers();
    
    SerialPutString("[GUI] Desktop ready. Main loop started.\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    // Main event loop
    MOUSE_STATE ms;
    int last_x = 320, last_y = 240;
    uint8_t last_buttons = 0;
    char buf[64];
    int need_redraw = 0;
    
    while(1) {
        // Process ALL PS/2 data (keyboard and mouse)
        while (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);
            
            if (status & 0x20) {
                // Mouse data
                MouseHandleInterrupt();
            }
            // Keyboard data is read and ignored
            (void)data;
        }
        
        // Get current mouse state
        MouseGetState(&ms);
        
        // Check if anything changed
        if (ms.x != last_x || ms.y != last_y || ms.buttons != last_buttons) {
            
            // Handle left mouse button press
            if (ms.left_down && !(last_buttons & 1)) {
                SerialPutString("[Mouse] Click at ");
                SerialPrintDec(ms.x);
                SerialPutString(", ");
                SerialPrintDec(ms.y);
                SerialPutString("\r\n");
                
                // Check "Spawn Window" button
                if (pt_in_rect(ms.x, ms.y, btn_x, btn_y, btn_w, btn_h)) {
                    SerialPutString("[GUI] Spawning new window\r\n");
                    
                    char title[16];
                    title[0] = 'W'; title[1] = 'i'; title[2] = 'n'; title[3] = 'd';
                    title[4] = 'o'; title[5] = 'w'; title[6] = ' ';
                    int n = window_counter;
                    if (n >= 10) title[7] = '0' + (n / 10) % 10;
                    else title[7] = ' ';
                    title[8] = '0' + n % 10;
                    title[9] = 0;
                    
                    int wx = 50 + (window_counter * 30) % 320;
                    int wy = 60 + (window_counter * 25) % 220;
                    
                    HANDLE hwnd = Win32kCreateWindow("TestWindow", title, wx, wy, 280, 180,
                                                      WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION);
                    Win32kShowWindow(hwnd);
                    Win32kUpdateWindow(hwnd);
                    window_counter++;
                    need_redraw = 1;
                }
                // Check "Clear All" button
                else if (pt_in_rect(ms.x, ms.y, btn2_x, btn2_y, btn2_w, btn2_h)) {
                    SerialPutString("[GUI] Clearing all windows\r\n");
                    VgaClearScreen(COLOR_BLUE);
                    window_counter = 0;
                    need_redraw = 1;
                }
                // Otherwise handle window interaction
                else {
                    Win32kHandleMouseDown(ms.x, ms.y, 1);
                }
            }
            
            // Handle left mouse button release
            if (!ms.left_down && (last_buttons & 1)) {
                Win32kHandleMouseUp(ms.x, ms.y, 1);
            }
            
            // Handle mouse drag
            if (ms.left_down) {
                Win32kHandleMouseMove(ms.x, ms.y);
            }
            
            // Erase old cursor
            MouseEraseCursor();
            
            // Redraw buttons
            VgaFillRect(btn_x, btn_y, btn_w, btn_h, COLOR_DARK_GRAY);
            VgaDrawRect(btn_x, btn_y, btn_w, btn_h, COLOR_WHITE);
            VgaDrawString(btn_x + 12, btn_y + 6, "Spawn Window", COLOR_WHITE, COLOR_DARK_GRAY);
            
            VgaFillRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_DARK_GRAY);
            VgaDrawRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_WHITE);
            VgaDrawString(btn2_x + 12, btn2_y + 6, "Clear All", COLOR_WHITE, COLOR_DARK_GRAY);
            
            // Update status bar
            VgaFillRect(0, 462, 640, 18, COLOR_DARK_GRAY);
            
            // Build status string
            int pos = 0;
            buf[pos++] = 'X'; buf[pos++] = ':';
            buf[pos++] = '0' + (ms.x / 100) % 10;
            buf[pos++] = '0' + (ms.x / 10) % 10;
            buf[pos++] = '0' + ms.x % 10;
            buf[pos++] = ' '; buf[pos++] = 'Y'; buf[pos++] = ':';
            buf[pos++] = '0' + (ms.y / 100) % 10;
            buf[pos++] = '0' + (ms.y / 10) % 10;
            buf[pos++] = '0' + ms.y % 10;
            buf[pos++] = ' '; buf[pos++] = 'W'; buf[pos++] = 'i'; buf[pos++] = 'n';
            buf[pos++] = 'd'; buf[pos++] = 'o'; buf[pos++] = 'w'; buf[pos++] = 's';
            buf[pos++] = ':'; buf[pos++] = ' ';
            if (window_counter >= 10) buf[pos++] = '0' + (window_counter / 10) % 10;
            buf[pos++] = '0' + window_counter % 10;
            buf[pos] = 0;
            
            VgaDrawString(4, 464, buf, COLOR_WHITE, COLOR_DARK_GRAY);
            VgaDrawString(160, 464, "Left click: buttons/windows  |  Keys: ignored", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
            
            // Draw cursor and flush
            MouseDrawCursor();
            VgaSwapBuffers();
            
            // Update last state
            last_x = ms.x;
            last_y = ms.y;
            last_buttons = ms.buttons;
            need_redraw = 0;
        }
        
        // If windows were modified (closed/destroyed), redraw everything
        if (need_redraw) {
            VgaClearScreen(COLOR_BLUE);
            Win32kRedrawAll();
            
            // Redraw buttons
            VgaFillRect(btn_x, btn_y, btn_w, btn_h, COLOR_DARK_GRAY);
            VgaDrawRect(btn_x, btn_y, btn_w, btn_h, COLOR_WHITE);
            VgaDrawString(btn_x + 12, btn_y + 6, "Spawn Window", COLOR_WHITE, COLOR_DARK_GRAY);
            
            VgaFillRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_DARK_GRAY);
            VgaDrawRect(btn2_x, btn2_y, btn2_w, btn2_h, COLOR_WHITE);
            VgaDrawString(btn2_x + 12, btn2_y + 6, "Clear All", COLOR_WHITE, COLOR_DARK_GRAY);
            
            MouseDrawCursor();
            VgaSwapBuffers();
            need_redraw = 0;
        }
        
        // Small delay to prevent CPU hogging
        for (volatile int i = 0; i < 500; i++);
    }
}