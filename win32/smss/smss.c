#include <stdint.h>
#include "smss.h"
#include "util.h"
#include "serial.h"
#include "portio.h"
#include "hal.h"
#include "cdfs.h"
#include "mm.h"
#include "peloader.h"
#include "fb.h"
#include "win32k.h"
#include "mouse.h"
#include "keyboard.h"
#include "guiapp.h"

typedef int (*CmdAppInitFn)(const GUI_APP_API *api);
typedef GUI_HANDLE (*CmdAppCreateMainWindowFn)(void);
typedef void (*CmdAppHandleKeyFn)(uint8_t scancode, char ascii, uint8_t pressed);
typedef int (*CmdAppShouldExitFn)(void);
typedef void (*CmdAppResetExitFn)(void);

typedef struct _CMD_APP_INSTANCE {
    void *image;
    GUI_HANDLE window;
    CmdAppHandleKeyFn handle_key;
    CmdAppShouldExitFn should_exit;
    CmdAppResetExitFn reset_exit;
} CMD_APP_INSTANCE;

static GUI_HANDLE smss_register_class(const char *className, uint32_t style, void (*wndProc)(GUI_HANDLE, uint32_t, uint32_t, uint32_t)) {
    return Win32kRegisterClass(className, style, (void (*)(HANDLE, uint32_t, uint32_t, uint32_t))wndProc);
}

static GUI_HANDLE smss_create_window(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) {
    return Win32kCreateWindow(className, title, x, y, w, h, style);
}

static void smss_show_window(GUI_HANDLE hwnd) { Win32kShowWindow(hwnd); }
static void smss_update_window(GUI_HANDLE hwnd) { Win32kUpdateWindow(hwnd); }
static void smss_get_client_rect(GUI_HANDLE hwnd, GUI_RECT *rect) { Win32kGetClientRect(hwnd, (RECT*)rect); }
static void smss_get_window_rect(GUI_HANDLE hwnd, GUI_RECT *rect) { Win32kGetWindowRect(hwnd, (RECT*)rect); }
static void smss_fill_rect(int x, int y, int w, int h, uint8_t color) { FbFillRect(x, y, w, h, color); }
static void smss_draw_rect(int x, int y, int w, int h, uint8_t color) { FbDrawRect(x, y, w, h, color); }
static void smss_draw_string(int x, int y, const char *str, uint8_t fg, uint8_t bg) { FbDrawString(x, y, str, fg, bg); }
static int smss_read_sector(uint32_t lba, uint8_t *buffer) { return CdfsReadSector(lba, buffer); }

static void smss_reboot(void) {
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile("int $0");
}

static void smss_shutdown(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

static int smss_execute_image(const char *path) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;
    int ret;

    if (!path || !*path) return -1;
    if (!CdfsReadFile(path, &file_buf, &file_size)) return -1;
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

    PeResolveImports(image);
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

static const GUI_APP_API gui_api = {
    smss_register_class,
    smss_create_window,
    smss_show_window,
    smss_update_window,
    smss_get_client_rect,
    smss_get_window_rect,
    smss_fill_rect,
    smss_draw_rect,
    smss_draw_string,
    smss_read_sector,
    smss_execute_image,
    smss_reboot,
    smss_shutdown
};

static int load_cmd_app(CMD_APP_INSTANCE *app) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    CmdAppInitFn init_fn;
    CmdAppCreateMainWindowFn create_window_fn;

    if (!app) return 0;

    if (!CdfsReadFile("/SYSTEM32/CMD.EXE", &file_buf, &file_size)) {
        SerialPutString("[SMSS] CMD.EXE not found in SYSTEM32\r\n");
        return 0;
    }

    app->image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    if (!app->image) {
        SerialPutString("[SMSS] Failed to load CMD.EXE\r\n");
        return 0;
    }

    PeResolveImports(app->image);
    if (*(uint32_t*)app->image != 0x464C457F) {
        PePerformRelocations(app->image);
    }

    init_fn = (CmdAppInitFn)PeGetProcAddress(app->image, "CmdAppInit");
    create_window_fn = (CmdAppCreateMainWindowFn)PeGetProcAddress(app->image, "CmdAppCreateMainWindow");
    app->handle_key = (CmdAppHandleKeyFn)PeGetProcAddress(app->image, "CmdAppHandleKey");
    app->should_exit = (CmdAppShouldExitFn)PeGetProcAddress(app->image, "CmdAppShouldExit");
    app->reset_exit = (CmdAppResetExitFn)PeGetProcAddress(app->image, "CmdAppResetExit");

    if (!init_fn || !create_window_fn || !app->handle_key || !app->should_exit || !app->reset_exit) {
        SerialPutString("[SMSS] CMD.EXE missing required exports\r\n");
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }

    if (!init_fn(&gui_api)) {
        SerialPutString("[SMSS] CMD.EXE init failed\r\n");
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }

    app->reset_exit();
    app->window = create_window_fn();
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

void SmssSessionRun(void *mb_info) {
    MOUSE_STATE mouse_state;
    int last_x = 320;
    int last_y = 240;
    uint8_t last_buttons = 0;
    KEYBOARD_EVENT key_event;
    int running = 1;
    CMD_APP_INSTANCE cmd_app;

    SerialPutString("[SMSS] Starting Win32 subsystem\r\n");

    cmd_app.image = 0;
    cmd_app.window = 0xFFFFFFFFU;
    cmd_app.handle_key = 0;
    cmd_app.should_exit = 0;
    cmd_app.reset_exit = 0;

    Win32kInit(mb_info);
    MouseInit();
    KeyboardInit();

    if (!load_cmd_app(&cmd_app)) {
        restore_text_mode();
        HalInitialize();
        HalClearScreen(0x1F);
        HalPutString("Failed to launch CMD.EXE from SYSTEM32.\n", 0x0C);
        return;
    }

    if (cmd_app.window != INVALID_HANDLE) Win32kShowWindow(cmd_app.window);
    Win32kRedrawAll();

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
                    if (Win32kIsDragging()) Win32kRedrawAll();
                    else Win32kRefreshCursor();
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
                KeyboardHandleData(data);
                while (KeyboardPollEvent(&key_event)) {
                    if (key_event.pressed && key_event.scancode == 0x01) running = 0;
                    if (cmd_app.handle_key) {
                        cmd_app.handle_key(key_event.scancode, key_event.ascii, key_event.pressed);
                    }
                    if (key_event.pressed) Win32kRedrawAll();
                    if (cmd_app.should_exit && cmd_app.should_exit()) running = 0;
                }
            }
        }

        for (volatile int i = 0; i < 3000; i++);
    }

    if (cmd_app.window != INVALID_HANDLE) {
        Win32kDestroyWindow(cmd_app.window);
    }
    if (cmd_app.image) {
        PeFreeImage(cmd_app.image);
    }

    restore_text_mode();
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("discouNT\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Returned from SMSS session.\n\n", 0x0A);
}
