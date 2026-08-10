#include <stdint.h>
#include "subsystem.h"
#include "subsystem.h"
#include "util.h"
#include "serial.h"
#include "portio.h"
#include "hal.h"
#include "fb.h"
#include "win32k.h"
#include "mouse.h"
#include "object.h"

static void *subsystem_mb_info = 0;

static HANDLE progman_class = INVALID_HANDLE;
static HANDLE smss_class = INVALID_HANDLE;
static HANDLE progman_window = INVALID_HANDLE;
static HANDLE smss_window = INVALID_HANDLE;

static void restore_text_mode(void) {
    outb(0x3C2, 0x67);

    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x00);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);

    outb(0x3D4, 0x11);
    outb(0x3D5, inb(0x3D5) & 0x7F);

    {
        uint8_t crtc[] = {
            0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
            0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
            0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF
        };
        for (int i = 0; i < 25; i++) {
            outb(0x3D4, i);
            outb(0x3D5, crtc[i]);
        }
    }

    {
        uint8_t gc[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x0F, 0xFF};
        for (int i = 0; i < 9; i++) {
            outb(0x3CE, i);
            outb(0x3CF, gc[i]);
        }
    }

    {
        uint8_t ac[] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
            0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
            0x0C, 0x00, 0x0F, 0x08, 0x00
        };
        inb(0x3DA);
        for (int i = 0; i < 21; i++) {
            outb(0x3C0, i);
            outb(0x3C0, ac[i]);
        }
        outb(0x3C0, 0x20);
    }

    {
        uint16_t *textbuf = (uint16_t*)0xB8000;
        for (int i = 0; i < 80 * 25; i++) textbuf[i] = 0x0F20;
    }
}

static void draw_program_icon(int x, int y, const char *label, uint8_t fill) {
    FbFillRect(x, y, 36, 28, fill);
    FbDrawRect(x, y, 36, 28, COLOR_BLACK);
    FbFillRect(x + 4, y + 4, 28, 18, COLOR_LIGHT_GRAY);
    FbDrawRect(x + 4, y + 4, 28, 18, COLOR_WHITE);
    FbDrawString(x - 4, y + 34, label, COLOR_WHITE, COLOR_BLUE);
}

static void progman_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == WM_PAINT) {
        RECT rect;
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        if (!win) return;

        Win32kGetClientRect(hwnd, &rect);

        FbFillRect(win->x + rect.left, win->y + rect.top,
                   rect.right - rect.left, rect.bottom - rect.top, COLOR_BLUE);

        FbDrawString(win->x + rect.left + 10, win->y + rect.top + 8,
                     "Program Manager", COLOR_WHITE, COLOR_BLUE);
        FbDrawString(win->x + rect.left + 10, win->y + rect.top + 22,
                     "discouNT Win32 subsystem", COLOR_LIGHT_GRAY, COLOR_BLUE);

        draw_program_icon(win->x + rect.left + 26,  win->y + rect.top + 54, "Main", COLOR_CYAN);
        draw_program_icon(win->x + rect.left + 86,  win->y + rect.top + 54, "System", COLOR_GREEN);
        draw_program_icon(win->x + rect.left + 146, win->y + rect.top + 54, "Shell", COLOR_MAGENTA);

        FbDrawString(win->x + rect.left + 10, win->y + rect.top + 110,
                     "Use the mouse to drag windows.", COLOR_YELLOW, COLOR_BLUE);
        FbDrawString(win->x + rect.left + 10, win->y + rect.top + 122,
                     "Press ESC to log off the session.", COLOR_YELLOW, COLOR_BLUE);

        ObDereferenceObject(hwnd);
    }
}

static void smss_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == WM_PAINT) {
        RECT rect;
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        if (!win) return;

        Win32kGetClientRect(hwnd, &rect);

        FbFillRect(win->x + rect.left, win->y + rect.top,
                   rect.right - rect.left, rect.bottom - rect.top, COLOR_LIGHT_GRAY);

        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 8,
                     "Windows Session Manager", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 26,
                     "Session 0 initialized successfully.", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 38,
                     "Subsystems: CSRSS, WIN32K, USER32, GDI32", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 62,
                     "SMSS.EXE launched the interactive desktop.", COLOR_BLUE, COLOR_LIGHT_GRAY);
        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 74,
                     "This is still kernel-hosted, but the GUI path is active.", COLOR_BLUE, COLOR_LIGHT_GRAY);
        FbDrawString(win->x + rect.left + 8, win->y + rect.top + 98,
                     "Press ESC to return to the native shell.", COLOR_RED, COLOR_LIGHT_GRAY);

        ObDereferenceObject(hwnd);
    }
}

static void destroy_session_windows(void) {
    if (smss_window != INVALID_HANDLE) {
        Win32kDestroyWindow(smss_window);
        smss_window = INVALID_HANDLE;
    }
    if (progman_window != INVALID_HANDLE) {
        Win32kDestroyWindow(progman_window);
        progman_window = INVALID_HANDLE;
    }
}

static void ensure_window_classes(void) {
    if (progman_class == INVALID_HANDLE) {
        progman_class = Win32kRegisterClass("ProgmanClass", 0, progman_wndproc);
    }
    if (smss_class == INVALID_HANDLE) {
        smss_class = Win32kRegisterClass("SmssClass", 0, smss_wndproc);
    }
}

void SubsystemInit(void *mb_info) {
    subsystem_mb_info = mb_info;
}

void SubsystemLaunchSmss(void) {
    MOUSE_STATE mouse_state;
    int last_x = 320;
    int last_y = 240;
    uint8_t last_buttons = 0;
    int running = 1;

    SerialPutString("[SMSS] Starting Win32 subsystem\r\n");

    Win32kInit(subsystem_mb_info);
    MouseInit();
    ensure_window_classes();

    progman_window = Win32kCreateWindow("ProgmanClass", "Program Manager",
                                        32, 28, 576, 300, WS_OVERLAPPEDWINDOW);
    smss_window = Win32kCreateWindow("SmssClass", "Session Manager",
                                     92, 118, 456, 152, WS_OVERLAPPEDWINDOW);

    if (progman_window != INVALID_HANDLE) Win32kShowWindow(progman_window);
    if (smss_window != INVALID_HANDLE) Win32kShowWindow(smss_window);
    Win32kRedrawAll();

    MouseGetState(&mouse_state);
    last_x = mouse_state.x;
    last_y = mouse_state.y;
    last_buttons = mouse_state.buttons;

    while (running) {
        if (inb(0x64) & 1) {
            uint8_t status = inb(0x64);

            if (status & 0x20) {
                MouseHandleInterrupt();
                MouseGetState(&mouse_state);

                if (mouse_state.x != last_x || mouse_state.y != last_y) {
                    Win32kHandleMouseMove(mouse_state.x, mouse_state.y);
                    last_x = mouse_state.x;
                    last_y = mouse_state.y;
                    Win32kRedrawAll();
                }

                if ((mouse_state.buttons & MOUSE_LEFT) && !(last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseDown(mouse_state.x, mouse_state.y, 1);
                }
                if (!(mouse_state.buttons & MOUSE_LEFT) && (last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseUp(mouse_state.x, mouse_state.y, 1);
                    Win32kRedrawAll();
                }

                last_buttons = mouse_state.buttons;
            } else {
                uint8_t data = inb(0x60);
                if (!(data & 0x80) && data == 0x01) running = 0;
            }
        }

        for (volatile int i = 0; i < 3000; i++);
    }

    destroy_session_windows();
    restore_text_mode();
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("discouNT\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Returned from SMSS session.\n\n", 0x0A);
}
