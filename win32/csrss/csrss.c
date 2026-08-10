#include <stdint.h>
#include "csrss.h"
#include "util.h"
#include "serial.h"
#include "portio.h"
#include "hal.h"
#include "cdfs.h"
#include "mm.h"
#include "peloader.h"
#include "fb.h"
#include "w32k.h"
#include "mouse.h"
#include "keyboard.h"
#include "guiapp.h"
#include "net.h"
#include "version.h"

typedef int (*GuiAppInitFn)(const GUI_APP_API *api);
typedef GUI_HANDLE (*GuiAppCreateMainWindowFn)(void);
typedef void (*GuiAppHandleKeyFn)(uint8_t scancode, char ascii, uint8_t pressed);
typedef void (*GuiAppHandleMouseFn)(int x, int y, uint8_t buttons, uint8_t event_type);
typedef int (*GuiAppShouldExitFn)(void);
typedef void (*GuiAppResetExitFn)(void);

typedef struct _GUI_APP_INSTANCE {
    uint32_t pid;
    void *image;
    GUI_HANDLE window;
    GuiAppHandleKeyFn handle_key;
    GuiAppHandleMouseFn handle_mouse;
    GuiAppShouldExitFn should_exit;
    GuiAppResetExitFn reset_exit;
} GUI_APP_INSTANCE;

#define MAX_GUI_APPS 8

static GUI_APP_INSTANCE g_gui_apps[MAX_GUI_APPS];
static int g_gui_app_count = 0;
static uint32_t g_next_gui_pid = 1;
static uint32_t g_current_gui_pid = 0;
static char g_error_app[96];
static char g_error_text[256];
static HANDLE g_error_class = INVALID_HANDLE;
static HANDLE g_error_window = INVALID_HANDLE;

static void uppercase_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i++] = c;
    }
    dst[i] = 0;
}

static void csrss_error_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == WM_PAINT) {
        RECT rect;
        Win32kGetClientRect(hwnd, &rect);
        FbFillRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, COLOR_LIGHT_GRAY);
        FbDrawString(rect.left + 10, rect.top + 12, g_error_app, COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(rect.left + 10, rect.top + 30, g_error_text, COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(rect.left + 10, rect.top + 54, "This program could not be started.", COLOR_DARK_GRAY, COLOR_LIGHT_GRAY);
    } else if (msg == WM_DESTROY) {
        g_error_window = INVALID_HANDLE;
    }
}

static void csrss_show_launch_error(const char *path, const char *detail) {
    if (g_error_class == INVALID_HANDLE) {
        g_error_class = Win32kRegisterClass("LoaderErrorDialog", 0, csrss_error_wndproc);
    }
    if (g_error_class == INVALID_HANDLE) return;

    if (path) {
        int i = 0;
        while (path[i] && i < (int)sizeof(g_error_app) - 1) {
            g_error_app[i] = path[i];
            i++;
        }
        g_error_app[i] = 0;
    } else {
        strcpy(g_error_app, "Application Error");
    }

    if (detail) {
        int i = 0;
        while (detail[i] && i < (int)sizeof(g_error_text) - 1) {
            g_error_text[i] = detail[i];
            i++;
        }
        g_error_text[i] = 0;
    } else {
        strcpy(g_error_text, "The application could not be started.");
    }

    if (g_error_window != INVALID_HANDLE) {
        Win32kDestroyWindow(g_error_window);
        g_error_window = INVALID_HANDLE;
    }

    g_error_window = Win32kCreateWindowByClass(g_error_class, "Application Error", 120, 120, 520, 120,
                                               WS_VISIBLE | WS_CAPTION | WS_SYSMENU);
    if (g_error_window != INVALID_HANDLE) {
        Win32kActivateWindow(g_error_window);
        Win32kShowWindow(g_error_window);
        Win32kRedrawAll();
    }
}

static GUI_HANDLE csrss_register_class(const char *className, uint32_t style, void (*wndProc)(GUI_HANDLE, uint32_t, uint32_t, uint32_t)) {
    return Win32kRegisterClass(className, style, (void (*)(HANDLE, uint32_t, uint32_t, uint32_t))wndProc);
}

static GUI_HANDLE csrss_create_window(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) {
    return Win32kCreateWindow(className, title, x, y, w, h, style);
}

static GUI_HANDLE csrss_create_window_by_class(GUI_HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style) {
    return Win32kCreateWindowByClass((HANDLE)hClass, title, x, y, w, h, style);
}

static void csrss_show_window(GUI_HANDLE hwnd) { Win32kShowWindow(hwnd); }
static void csrss_update_window(GUI_HANDLE hwnd) { Win32kUpdateWindow(hwnd); }
static void csrss_get_client_rect(GUI_HANDLE hwnd, GUI_RECT *rect) { Win32kGetClientRect(hwnd, (RECT*)rect); }
static void csrss_get_window_rect(GUI_HANDLE hwnd, GUI_RECT *rect) { Win32kGetWindowRect(hwnd, (RECT*)rect); }
static void csrss_fill_rect(int x, int y, int w, int h, uint8_t color) { FbFillRect(x, y, w, h, color); }
static void csrss_draw_rect(int x, int y, int w, int h, uint8_t color) { FbDrawRect(x, y, w, h, color); }
static void csrss_draw_string(int x, int y, const char *str, uint8_t fg, uint8_t bg) { FbDrawString(x, y, str, fg, bg); }
static int csrss_read_sector(uint32_t lba, uint8_t *buffer) { return CdfsReadSector(lba, buffer); }
static uint32_t csrss_get_process_id(void) { return g_current_gui_pid; }
static int csrss_get_screen_width(void) { return FbGetWidth(); }
static int csrss_get_screen_height(void) { return FbGetHeight(); }
static int csrss_get_screen_mode_count(void) { return FbGetModeCount(); }
static int csrss_get_screen_mode_info(int index, int *width, int *height, int *bpp) {
    return FbGetModeInfo(index, width, height, bpp);
}
static int csrss_ping(const char *ip_text, char *out_text, int out_text_len) {
    return NetPing(ip_text, out_text, out_text_len);
}

static int csrss_load_gui_instance(const char *path, GUI_APP_INSTANCE *app);

static int csrss_spawn_gui_instance(const char *path) {
    GUI_APP_INSTANCE app;

    if (g_gui_app_count >= MAX_GUI_APPS) return -5;

    app.pid = g_next_gui_pid++;
    app.image = 0;
    app.window = 0xFFFFFFFFU;
    app.handle_key = 0;
    app.handle_mouse = 0;
    app.should_exit = 0;
    app.reset_exit = 0;

    if (!csrss_load_gui_instance(path, &app)) return -1;

    if (app.window != INVALID_HANDLE) {
        WINDOW *win = (WINDOW*)ObReferenceObject(app.window);
        if (win) {
            int cascade = g_gui_app_count * 24;
            int screen_w = FbGetWidth();
            int screen_h = FbGetHeight();

            if (screen_w <= 0) screen_w = 640;
            if (screen_h <= 0) screen_h = 480;

            win->x += cascade;
            win->y += cascade;

            if (win->x + win->width > screen_w) win->x = screen_w - win->width;
            if (win->y + win->height > screen_h) win->y = screen_h - win->height;
            if (win->x < 0) win->x = 0;
            if (win->y < 0) win->y = 0;

            ObDereferenceObject(app.window);
        }

        Win32kActivateWindow(app.window);
        Win32kShowWindow(app.window);
    }

    g_gui_apps[g_gui_app_count++] = app;
    Win32kRedrawAll();
    return 0;
}

static int csrss_execute_image(const char *path) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;
    int ret;
    char upper_path[256];

    if (!path || !*path) return -1;

    uppercase_copy(upper_path, path, sizeof(upper_path));
    if (csrss_spawn_gui_instance(upper_path) == 0) {
        return 0;
    }

    if (!CdfsReadFile(upper_path, &file_buf, &file_size)) return -1;
    if (((file_size < 64 || file_buf[0] != 0x4D || file_buf[1] != 0x5A) &&
         (file_size < 4 || *(uint32_t*)file_buf != 0x464C457F))) {
        kfree(file_buf);
        return -2;
    }

    image = PeLoadImage(file_buf, file_size);
    if (!image) {
        kfree(file_buf);
        return -3;
    }

    if (!PeResolveImports(image)) {
        csrss_show_launch_error(upper_path, PeGetLastError());
        PeFreeImage(image);
        kfree(file_buf);
        return -5;
    }
    if (!(*(uint32_t*)file_buf == 0x464C457F)) {
        PePerformRelocations(image);
    }

    {
        uint8_t *exe_stack = (uint8_t*)kmalloc(65536);
        uint32_t exe_esp;
        uint32_t saved_esp;
        typedef int (*EntryFunc)(void);
        EntryFunc func = (EntryFunc)PeGetEntryPoint(image);

        if (!func || !exe_stack) {
            if (exe_stack) kfree(exe_stack);
            PeFreeImage(image);
            kfree(file_buf);
            return -4;
        }

        exe_esp = (uint32_t)(exe_stack + 65536 - 256);
        __asm__ volatile(
            "movl %%esp, %[oldsp]\n"
            "movl %[newsp], %%esp\n"
            "call *%[fn]\n"
            "movl %%eax, %[retval]\n"
            "movl %[oldsp], %%esp\n"
            :
              [oldsp] "=&r"(saved_esp),
              [retval] "=r"(ret)
            : [newsp] "r"(exe_esp),
              [fn] "r"(func)
            : "eax", "ecx", "edx", "memory"
        );
        kfree(exe_stack);
    }

    PeFreeImage(image);
    kfree(file_buf);
    return ret;
}

static int csrss_set_screen_resolution(int width, int height) {
    if (!FbSetResolution(width, height, 32)) return 0;

    for (int i = 0; i < g_gui_app_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(g_gui_apps[i].window);
        int screen_w;
        int screen_h;
        if (!win) continue;

        screen_w = FbGetWidth();
        screen_h = FbGetHeight();
        if (screen_w <= 0) screen_w = 640;
        if (screen_h <= 0) screen_h = 480;

        if (win->maximized) {
            win->x = 0;
            win->y = 0;
            win->width = screen_w;
            win->height = screen_h;
        } else {
            if (win->x + win->width > screen_w) win->x = screen_w - win->width;
            if (win->y + win->height > screen_h) win->y = screen_h - win->height;
            if (win->x < 0) win->x = 0;
            if (win->y < 0) win->y = 0;
        }
        ObDereferenceObject(g_gui_apps[i].window);
    }

    Win32kRedrawAll();
    return 1;
}

static void csrss_reboot(void) {
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile("int $0");
}

static void csrss_shutdown(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

static const GUI_APP_API gui_api = {
    csrss_register_class,
    csrss_create_window,
    csrss_create_window_by_class,
    csrss_show_window,
    csrss_update_window,
    csrss_get_client_rect,
    csrss_get_window_rect,
    csrss_fill_rect,
    csrss_draw_rect,
    csrss_draw_string,
    csrss_read_sector,
    csrss_execute_image,
    csrss_get_process_id,
    csrss_get_screen_width,
    csrss_get_screen_height,
    csrss_get_screen_mode_count,
    csrss_get_screen_mode_info,
    csrss_set_screen_resolution,
    csrss_ping,
    csrss_reboot,
    csrss_shutdown
};

static int csrss_load_gui_instance(const char *path, GUI_APP_INSTANCE *app) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    GuiAppInitFn init_fn;
    GuiAppCreateMainWindowFn create_window_fn;

    if (!app || !path || !*path) return 0;

    if (!CdfsReadFile(path, &file_buf, &file_size)) {
        SerialPutString("[CSRSS] GUI app not found: ");
        SerialPutString(path);
        SerialPutString("\r\n");
        return 0;
    }

    app->image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    if (!app->image) {
        SerialPutString("[CSRSS] Failed to load GUI app image\r\n");
        csrss_show_launch_error(path, "The application image could not be mapped.");
        return 0;
    }

    if (!PeResolveImports(app->image)) {
        csrss_show_launch_error(path, PeGetLastError());
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }
    if (*(uint32_t*)app->image != 0x464C457F) {
        PePerformRelocations(app->image);
    }

    init_fn = (GuiAppInitFn)PeGetProcAddress(app->image, "CmdAppInit");
    create_window_fn = (GuiAppCreateMainWindowFn)PeGetProcAddress(app->image, "CmdAppCreateMainWindow");
    app->handle_key = (GuiAppHandleKeyFn)PeGetProcAddress(app->image, "CmdAppHandleKey");
    app->handle_mouse = (GuiAppHandleMouseFn)PeGetProcAddress(app->image, "CmdAppHandleMouse");
    app->should_exit = (GuiAppShouldExitFn)PeGetProcAddress(app->image, "CmdAppShouldExit");
    app->reset_exit = (GuiAppResetExitFn)PeGetProcAddress(app->image, "CmdAppResetExit");

    if (!init_fn || !create_window_fn || !app->handle_key || !app->should_exit || !app->reset_exit) {
        SerialPutString("[CSRSS] GUI app missing required exports\r\n");
        csrss_show_launch_error(path, "The application is not a valid Win32 program.");
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }

    g_current_gui_pid = app->pid;
    if (!init_fn(&gui_api)) {
        SerialPutString("[CSRSS] GUI app init failed\r\n");
        g_current_gui_pid = 0;
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }

    app->reset_exit();
    g_current_gui_pid = app->pid;
    app->window = create_window_fn();
    g_current_gui_pid = 0;
    return (app->window != 0xFFFFFFFFU);
}

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

void CsrssSessionRun(void *mb_info) {
    MOUSE_STATE mouse_state;
    int last_x = 320;
    int last_y = 240;
    uint8_t last_buttons = 0;
    KEYBOARD_EVENT key_event;
    int running = 1;
    HANDLE active_hwnd;

    SerialPutString("[CSRSS] Starting Win32 subsystem session\r\n");

    g_gui_app_count = 0;
    g_next_gui_pid = 1;
    g_current_gui_pid = 0;

    Win32kInit(mb_info);
    MouseInit();
    KeyboardInit();

    if (csrss_spawn_gui_instance("/SYSTEM32/CMD.EXE") != 0) {
        restore_text_mode();
        HalInitialize();
        HalClearScreen(0x1F);
        HalPutString("Failed to launch CMD.EXE from SYSTEM32.\n", 0x0C);
        return;
    }

    MouseGetState(&mouse_state);
    last_x = mouse_state.x;
    last_y = mouse_state.y;
    last_buttons = mouse_state.buttons;

    while (running) {
        while (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);

            if (status & 0x20) {
                MouseHandleByte(data);
                MouseGetState(&mouse_state);

                if (mouse_state.x != last_x || mouse_state.y != last_y) {
                    Win32kHandleMouseMove(mouse_state.x, mouse_state.y);
                    last_x = mouse_state.x;
                    last_y = mouse_state.y;
                    Win32kRefreshCursor();
                }

                if ((mouse_state.buttons & MOUSE_LEFT) && !(last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseDown(mouse_state.x, mouse_state.y, 1);
                    active_hwnd = Win32kGetActiveWindow();
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd && g_gui_apps[i].handle_mouse) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                int client_right = win->x + win->width - 3;
                                int client_bottom = win->y + win->height - 3;

                                if (!win->minimized &&
                                    mouse_state.x >= client_left && mouse_state.x < client_right &&
                                    mouse_state.y >= client_top && mouse_state.y < client_bottom) {
                                    g_current_gui_pid = g_gui_apps[i].pid;
                                    g_gui_apps[i].handle_mouse(mouse_state.x - client_left,
                                                               mouse_state.y - client_top,
                                                               mouse_state.buttons,
                                                               GUI_MOUSE_LDOWN);
                                    g_current_gui_pid = 0;
                                }
                                ObDereferenceObject(active_hwnd);
                            }
                            break;
                        }
                    }
                }

                if (!(mouse_state.buttons & MOUSE_LEFT) && (last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseUp(mouse_state.x, mouse_state.y, 1);
                    active_hwnd = Win32kGetActiveWindow();
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd && g_gui_apps[i].handle_mouse) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                int client_right = win->x + win->width - 3;
                                int client_bottom = win->y + win->height - 3;

                                if (!win->minimized &&
                                    mouse_state.x >= client_left && mouse_state.x < client_right &&
                                    mouse_state.y >= client_top && mouse_state.y < client_bottom) {
                                    g_current_gui_pid = g_gui_apps[i].pid;
                                    g_gui_apps[i].handle_mouse(mouse_state.x - client_left,
                                                               mouse_state.y - client_top,
                                                               mouse_state.buttons,
                                                               GUI_MOUSE_LUP);
                                    g_current_gui_pid = 0;
                                }
                                ObDereferenceObject(active_hwnd);
                            }
                            break;
                        }
                    }
                    Win32kRedrawAll();
                }

                last_buttons = mouse_state.buttons;
            } else {
                KeyboardHandleData(data);
                while (KeyboardPollEvent(&key_event)) {
                    if (key_event.pressed && key_event.scancode == 0x01) running = 0;
                    active_hwnd = Win32kGetActiveWindow();
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd && g_gui_apps[i].handle_key) {
                            g_current_gui_pid = g_gui_apps[i].pid;
                            g_gui_apps[i].handle_key(key_event.scancode, key_event.ascii, key_event.pressed);
                            g_current_gui_pid = 0;
                            break;
                        }
                    }
                    if (key_event.pressed) Win32kRedrawAll();
                }
            }
        }

        for (int i = 0; i < g_gui_app_count; ) {
            int remove = 0;

            if (!ObReferenceObject(g_gui_apps[i].window)) {
                remove = 1;
            } else {
                ObDereferenceObject(g_gui_apps[i].window);
                if (g_gui_apps[i].should_exit && g_gui_apps[i].should_exit()) {
                    if (g_gui_apps[i].window != INVALID_HANDLE) {
                        Win32kDestroyWindow(g_gui_apps[i].window);
                    }
                    remove = 1;
                }
            }

            if (remove) {
                if (g_gui_apps[i].image) {
                    PeFreeImage(g_gui_apps[i].image);
                }
                for (int j = i; j < g_gui_app_count - 1; j++) {
                    g_gui_apps[j] = g_gui_apps[j + 1];
                }
                g_gui_app_count--;
                Win32kRedrawAll();
                continue;
            }
            i++;
        }

        if (g_gui_app_count == 0) running = 0;

        for (volatile int i = 0; i < 3000; i++);
    }

    for (int i = 0; i < g_gui_app_count; i++) {
        if (g_gui_apps[i].window != INVALID_HANDLE && ObReferenceObject(g_gui_apps[i].window)) {
            ObDereferenceObject(g_gui_apps[i].window);
            Win32kDestroyWindow(g_gui_apps[i].window);
        }
        if (g_gui_apps[i].image) {
            PeFreeImage(g_gui_apps[i].image);
        }
    }

    restore_text_mode();
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString(DISCOUNT_NAME "\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Returned from CSRSS session.\n\n", 0x0A);
}
