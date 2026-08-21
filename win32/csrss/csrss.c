#include <stdint.h>
#include "csrss.h"
#include "core/util.h"
#include "serial.h"
#include "arch/x86/portio.h"
#include "hal.h"
#include "cdfs.h"
#include "mm/mm.h"
#include "loader/peloader.h"
#include "fb.h"
#include "w32k.h"
#include "mouse.h"
#include "keyboard.h"
#include "guiapp.h"
#include "net.h"
#include "core/ke.h"
#include "core/version.h"
#include "core/bugcheck.h"

typedef int (*GuiAppInitFn)(const GUI_APP_API *api);
typedef GUI_HANDLE (*GuiAppCreateMainWindowFn)(void);
typedef void (*GuiAppHandleKeyFn)(uint8_t scancode, char ascii, uint8_t pressed);
typedef void (*GuiAppHandleMouseFn)(int x, int y, uint8_t buttons, uint8_t event_type);
typedef int (*GuiAppShouldExitFn)(void);
typedef void (*GuiAppResetExitFn)(void);
typedef int (*GuiWinMainFn)(void *hInstance, void *hPrevInstance, char *lpCmdLine, int nCmdShow);
typedef int (*GuiMainFn)(void);
typedef int (*User32PostMessageWFn)(GUI_HANDLE hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);
typedef GUI_HANDLE (*User32FindTopLevelWindowForProcessIdFn)(uint32_t pid);
typedef void (*User32InjectKeyboardFn)(GUI_HANDLE hWnd, uint32_t key, int pressed);
typedef void (*User32InjectMouseFn)(GUI_HANDLE hWnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
typedef void (*User32SetProcessIdFn)(uint32_t pid);
typedef void (*Kernel32SetProcessImageBaseFn)(void *image_base);
typedef void (*Kernel32SetConsoleSinkFn)(void (*sink)(const char *, uint32_t));
typedef int (*WlxNegotiateFn)(uint32_t version, uint32_t *dll_version);
typedef int (*WlxInitializeFn)(void *station, void *dispatch, void *context,
                               void *options, void **gina_context);
typedef int (*WlxLoggedOnSASFn)(void *gina_context, uint32_t sas_type, void *info);
typedef int (*WlxLoggedOutSASFn)(void *gina_context, uint32_t sas_type, void *info,
                                void *logon_sid, void *options, void *token,
                                void *mpr_notify, void *profile);
typedef int (*WlxLogoffFn)(void *gina_context);
typedef int (*WlxShutdownFn)(void *gina_context, uint32_t action);
typedef void (*WlxDisplaySASNoticeFn)(void *gina_context);
typedef intptr_t (*User32DialogBoxParamFn)(void *, const void *, void *, void *, intptr_t);
void CsrssGinaShowLogon(void);

static int csrss_wlx_dialog_box_param(void *hWlx, void *instance,
                                       const void *template_name,
                                       void *parent, void *dialog_proc,
                                       intptr_t init_param) {
    static User32DialogBoxParamFn dialog_box;
    if (!dialog_box) {
        void *user32 = PeGetLoadedModuleHandle("USER32.DLL");
        if (user32)
            dialog_box = (User32DialogBoxParamFn)PeGetProcAddress(user32, "DialogBoxParamW");
    }
    if (dialog_box)
        return (int)dialog_box(instance, template_name, parent, dialog_proc, init_param);
    (void)hWlx;
    CsrssGinaShowLogon();
    return 0;
}

static int csrss_wlx_use_ctrl_alt_del(void *hWlx) {
    (void)hWlx;
    return 1;
}

static void csrss_wlx_sas_notify(void *hWlx, uint32_t sasType) {
    (void)hWlx;
    (void)sasType;
}

static int csrss_wlx_message_box(void *hWlx, void *hwnd, const void *text,
                                  const void *caption, uint32_t type) {
    (void)hWlx; (void)hwnd; (void)text; (void)caption; (void)type;
    return 1;
}

/* The first members match WLX_DISPATCH_VERSION_1_3.  Keeping this table in
 * CSRSS gives a real GINA the Winlogon callbacks it expects during init. */
static struct {
    int (*WlxUseCtrlAltDel)(void *);
    void *WlxSetContextPointer;
    void (*WlxSasNotify)(void *, uint32_t);
    void *WlxSetTimeout;
    void *WlxAssignShellProtection;
    void *WlxMessageBox;
    void *WlxDialogBox;
    void *WlxDialogBoxParam;
    void *WlxDialogBoxIndirect;
    void *WlxDialogBoxIndirectParam;
    void *WlxSwitchDesktopToUser;
    void *WlxSwitchDesktopToWinlogon;
    void *WlxChangePasswordNotify;
    void *WlxGetSourceDesktop;
    void *WlxSetReturnDesktop;
    void *WlxCreateUserDesktop;
    void *WlxChangePasswordNotifyEx;
    void *WlxCloseUserDesktop;
    void *WlxSetOption;
    void *WlxGetOption;
    void *WlxWin31Migrate;
    void *WlxQueryClientCredentials;
    void *WlxQueryInetConnectorCredentials;
    void *WlxDisconnect;
    void *WlxQueryTerminalServicesData;
} g_wlx_dispatch = {
    .WlxUseCtrlAltDel = csrss_wlx_use_ctrl_alt_del,
    .WlxSasNotify = csrss_wlx_sas_notify,
    .WlxMessageBox = csrss_wlx_message_box,
    .WlxDialogBoxParam = csrss_wlx_dialog_box_param
};

typedef enum _CSRSS_SESSION_STATE {
    CSRSS_SESSION_BOOTING = 0,
    CSRSS_SESSION_LOGGED_ON,
    CSRSS_SESSION_LOCKED,
    CSRSS_SESSION_LOGGING_OFF,
    CSRSS_SESSION_SHUTTING_DOWN
} CSRSS_SESSION_STATE;

typedef enum _GUI_APP_KIND {
    GUI_APP_KIND_CUSTOM = 0,
    GUI_APP_KIND_WINMAIN,
    GUI_APP_KIND_MAIN
} GUI_APP_KIND;

typedef struct _GUI_APP_INSTANCE {
    uint32_t pid;
    char path[256];
    void *image;
    GUI_HANDLE window;
    HANDLE thread;
    GUI_APP_KIND kind;
    volatile int exited;
    int exit_code;
    GuiAppHandleKeyFn handle_key;
    GuiAppHandleMouseFn handle_mouse;
    GuiAppShouldExitFn should_exit;
    GuiAppResetExitFn reset_exit;
} GUI_APP_INSTANCE;

typedef struct _GUI_APP_THREAD_CTX {
    GUI_APP_INSTANCE *app;
    void *entry;
    GUI_APP_KIND kind;
} GUI_APP_THREAD_CTX;

#define MAX_GUI_APPS 8

static GUI_APP_INSTANCE g_gui_apps[MAX_GUI_APPS];
static int g_gui_app_count = 0;
static uint32_t g_next_gui_pid = 1;
static uint32_t g_current_gui_pid = 0;
static char g_error_app[96];
static char g_error_text[256];
static HANDLE g_error_class = INVALID_HANDLE;
static HANDLE g_error_window = INVALID_HANDLE;
static HANDLE g_mouse_capture = INVALID_HANDLE;
static int g_pending_error = 0;
static char g_pending_error_app[96];
static char g_pending_error_text[256];
static int g_pending_launch = 0;
static char g_pending_launch_path[256];
static User32PostMessageWFn g_user32_post_message = 0;
static User32FindTopLevelWindowForProcessIdFn g_user32_find_top_level_window = 0;
static User32InjectKeyboardFn g_user32_inject_keyboard = 0;
static User32InjectMouseFn g_user32_inject_mouse = 0;
static User32SetProcessIdFn g_user32_set_process_id = 0;
static Kernel32SetProcessImageBaseFn g_kernel32_set_process_image_base = 0;
static Kernel32SetConsoleSinkFn g_kernel32_set_console_sink = 0;
static WlxNegotiateFn g_wlx_negotiate = 0;
static WlxInitializeFn g_wlx_initialize = 0;
static WlxLoggedOnSASFn g_wlx_logged_on_sas = 0;
static WlxLoggedOutSASFn g_wlx_logged_out_sas = 0;
static WlxLogoffFn g_wlx_logoff = 0;
static WlxShutdownFn g_wlx_shutdown = 0;
static WlxDisplaySASNoticeFn g_wlx_display_sas_notice = 0;
static void *g_gina_context = 0;
static GUI_HANDLE g_session_class = INVALID_HANDLE;
static GUI_HANDLE g_lock_window = INVALID_HANDLE;
static GUI_HANDLE g_logon_window = INVALID_HANDLE;
static CSRSS_SESSION_STATE g_session_state = CSRSS_SESSION_BOOTING;
static uint8_t g_ctrl_down = 0;
static uint8_t g_alt_down = 0;
static int g_logon_field = 0;
static int g_shell_started = 0;
static char g_logon_user[32];
static char g_logon_password[32];

#define WLX_VERSION_1_3             0x00010003U
#define WLX_SAS_TYPE_CTRL_ALT_DEL   0x00000001U
#define WLX_SAS_TYPE_TIMEOUT        0x00000002U
#define WLX_SHUTDOWN_LOGOFF        0x00000001U
#define WLX_SHUTDOWN_REBOOT        0x00000004U

#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_CHAR         0x0102
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_MOUSEWHEEL   0x020A
#define MK_LBUTTON      0x0001
#define MAKELPARAM(l,h) ((uint32_t)(((uint16_t)(l)) | (((uint32_t)(uint16_t)(h)) << 16)))

static void uppercase_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i++] = c;
    }
    dst[i] = 0;
}

static GUI_APP_INSTANCE *csrss_find_app_by_window(GUI_HANDLE hwnd) {
    for (int i = 0; i < g_gui_app_count; i++) {
        if (g_gui_apps[i].window == hwnd) return &g_gui_apps[i];
    }
    return 0;
}

static uint32_t csrss_translate_key_wparam(const KEYBOARD_EVENT *event) {
    if (!event) return 0;

    switch (event->scancode) {
    case 0x0E: return 0x08;
    case 0x0F: return 0x09;
    case 0x1C: return 0x0D;
    case 0x01: return 0x1B;
    case 0x39: return 0x20;
    default:   return event->ascii ? (uint8_t)event->ascii : 0;
    }
}

static void csrss_post_standard_message(GUI_HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    if (!g_user32_post_message || hwnd == INVALID_HANDLE) return;
    g_user32_post_message(hwnd, msg, wParam, lParam);
}

static void csrss_inject_logon_mouse(GUI_HANDLE hwnd, uint32_t msg,
                                     uint32_t wParam, uint32_t lParam) {
    if (g_user32_inject_mouse)
        g_user32_inject_mouse(hwnd, msg, wParam, lParam);
    else
        csrss_post_standard_message(hwnd, msg, wParam, lParam);
}

static int csrss_post_logon_mouse(uint32_t msg, uint32_t wParam, int x, int y) {
    GUI_HANDLE hwnd;
    WINDOW *win;
    int client_left, client_top;

    if (g_session_state != CSRSS_SESSION_BOOTING || !g_user32_post_message)
        return 0;
    hwnd = g_user32_find_top_level_window ?
           g_user32_find_top_level_window(1) : Win32kGetActiveWindow();
    if (hwnd == INVALID_HANDLE)
        return 0;
    win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win)
        return 0;
    client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
    client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
    if (win->minimized) {
        ObDereferenceObject(hwnd);
        return 0;
    }
    csrss_inject_logon_mouse(hwnd, msg, wParam,
                             MAKELPARAM(x - client_left, y - client_top));
    ObDereferenceObject(hwnd);
    return 1;
}

static void csrss_error_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == WM_PAINT) {
        RECT rect;
        WINDOW *win;
        int client_left;
        int client_top;
        Win32kGetClientRect(hwnd, &rect);
        client_left = 0;
        client_top = 0;
        win = (WINDOW*)ObReferenceObject(hwnd);
        if (win) {
            client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
            client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
            ObDereferenceObject(hwnd);
        }
        FbFillRect(client_left, client_top, rect.right - rect.left, rect.bottom - rect.top, COLOR_LIGHT_GRAY);
        FbDrawString(client_left + 10, client_top + 12, g_error_app, COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(client_left + 10, client_top + 30, g_error_text, COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(client_left + 10, client_top + 54, "This program could not be started.", COLOR_DARK_GRAY, COLOR_LIGHT_GRAY);
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

static void csrss_lock_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam; (void)lParam;
    if (msg == WM_PAINT) {
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        int x = win ? win->x : 0;
        int y = win ? win->y : 0;
        int w = win ? win->width : FbGetWidth();
        int h = win ? win->height : FbGetHeight();
        if (win) ObDereferenceObject(hwnd);
        FbFillRect(x, y, w, h, COLOR_BLUE);
        FbDrawString(x + 24, y + h / 2 - 18, "This workstation is locked.", COLOR_WHITE, COLOR_BLUE);
        FbDrawString(x + 24, y + h / 2 + 2, "Press Ctrl+Alt+Enter to unlock.", COLOR_WHITE, COLOR_BLUE);
    } else if (msg == WM_DESTROY) {
        g_lock_window = INVALID_HANDLE;
    }
}

static void csrss_logon_wndproc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam; (void)lParam;
    if (msg == WM_PAINT) {
        WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
        int x = win ? win->x : 220;
        int y = win ? win->y : 160;
        int w = win ? win->width : 360;
        int h = win ? win->height : 220;
        char masked[32];
        int i;
        if (win) ObDereferenceObject(hwnd);
        for (i = 0; i < (int)sizeof(masked) - 1 && g_logon_password[i]; i++) masked[i] = '*';
        masked[i] = 0;
        FbFillRect(x, y, w, h, COLOR_LIGHT_GRAY);
        FbDrawString(x + 18, y + 28, "Welcome to discouNT", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(x + 18, y + 62, "User name:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(x + 122, y + 62, g_logon_user, COLOR_BLACK, COLOR_WHITE);
        FbDrawString(x + 18, y + 94, "Password:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        FbDrawString(x + 122, y + 94, masked, COLOR_BLACK, COLOR_WHITE);
        FbDrawString(x + 18, y + 142, "Press Enter to log on", COLOR_DARK_GRAY, COLOR_LIGHT_GRAY);
        FbDrawString(x + 18, y + 174, "Tab switches fields", COLOR_DARK_GRAY, COLOR_LIGHT_GRAY);
    } else if (msg == WM_DESTROY) {
        g_logon_window = INVALID_HANDLE;
    }
}

static void csrss_show_logon_screen(void) {
    if (g_logon_window != INVALID_HANDLE) return;
    g_session_class = Win32kRegisterClass("CsrssLogonScreen", 0, csrss_logon_wndproc);
    if (g_session_class == INVALID_HANDLE) return;
    g_logon_window = Win32kCreateWindowByClass(g_session_class, "Windows Logon", 220, 160, 360, 220,
                                                WS_VISIBLE | WS_CAPTION | WS_SYSMENU);
    if (g_logon_window != INVALID_HANDLE) {
        Win32kActivateWindow(g_logon_window);
        Win32kShowWindow(g_logon_window);
        Win32kRedrawAll();
    }
}

/* Entry used by the compatible MSGINA implementation. In a full Winlogon
 * build this is supplied by the Winlogon dispatch table; for discouNT it is
 * the narrow bridge from WlxLoggedOutSAS to the interactive desktop. */
void CsrssGinaShowLogon(void) {
    SerialPutString("[WINLOGON] MSGINA requested logon desktop\r\n");
    /* MSGINA owns the interactive dialog.  The old CSRSS-painted logon
     * window consumed keyboard input before USER32 could deliver it to the
     * edit controls, leaving the real GINA dialog read-only. */
}

static void csrss_complete_logon(void) {
    if (g_session_state != CSRSS_SESSION_BOOTING) return;
    g_session_state = CSRSS_SESSION_LOGGED_ON;
    if (g_logon_window != INVALID_HANDLE) {
        Win32kDestroyWindow(g_logon_window);
        g_logon_window = INVALID_HANDLE;
    }
    Win32kRedrawAll();
}

static void csrss_handle_logon_key(const KEYBOARD_EVENT *event) {
    char *field;
    int len;
    if (!event || !event->pressed) return;
    if (event->scancode == 0x0F) {
        g_logon_field = !g_logon_field;
        Win32kRedrawAll();
        return;
    }
    if (event->scancode == 0x1C) {
        csrss_complete_logon();
        return;
    }
    field = g_logon_field ? g_logon_password : g_logon_user;
    len = strlen(field);
    if (event->scancode == 0x0E) {
        if (len > 0) field[len - 1] = 0;
    } else if (event->ascii >= 32 && event->ascii < 127 && len < 31) {
        field[len] = event->ascii;
        field[len + 1] = 0;
    }
    Win32kRedrawAll();
}

static void csrss_show_lock_screen(void) {
    if (g_lock_window != INVALID_HANDLE) return;
    if (g_session_class == INVALID_HANDLE)
        g_session_class = Win32kRegisterClass("CsrssLockScreen", 0, csrss_lock_wndproc);
    if (g_session_class == INVALID_HANDLE) return;
    g_lock_window = Win32kCreateWindowByClass(g_session_class, "Windows Security", 0, 0,
                                               FbGetWidth(), FbGetHeight(), WS_VISIBLE);
    if (g_lock_window != INVALID_HANDLE) {
        Win32kActivateWindow(g_lock_window);
        Win32kShowWindow(g_lock_window);
        Win32kRedrawAll();
    }
}

static void csrss_hide_lock_screen(void) {
    if (g_lock_window != INVALID_HANDLE) {
        Win32kDestroyWindow(g_lock_window);
        g_lock_window = INVALID_HANDLE;
    }
    if (g_gui_app_count > 0 && g_gui_apps[0].window != INVALID_HANDLE)
        Win32kActivateWindow(g_gui_apps[0].window);
    Win32kRedrawAll();
}

static void csrss_session_lock(void) {
    if (g_session_state != CSRSS_SESSION_LOGGED_ON) return;
    g_session_state = CSRSS_SESSION_LOCKED;
    if (g_wlx_logged_on_sas) g_wlx_logged_on_sas(g_gina_context, WLX_SAS_TYPE_CTRL_ALT_DEL, 0);
    if (g_wlx_display_sas_notice) g_wlx_display_sas_notice(g_gina_context);
    csrss_show_lock_screen();
}

static void csrss_session_unlock(void) {
    if (g_session_state != CSRSS_SESSION_LOCKED) return;
    g_session_state = CSRSS_SESSION_LOGGED_ON;
    csrss_hide_lock_screen();
}

static void csrss_handle_secure_attention(void) {
    if (g_session_state == CSRSS_SESSION_LOGGED_ON) csrss_session_lock();
    else if (g_session_state == CSRSS_SESSION_LOCKED) csrss_session_unlock();
}

static HANDLE g_winlogon_thread = INVALID_HANDLE;

static void csrss_winlogon_sas_thread(void *arg) {
    uint32_t authentication_id[2] = {0, 0};
    uint8_t logon_sid[68] = {0};
    uint32_t options = 0;
    void *user_token = 0;
    void *profile = 0;
    struct { void *user; void *domain; void *password; void *old_password; } mpr = {0, 0, 0, 0};
    int sas_action;
    (void)arg;

    SerialPutString("[WINLOGON] Calling WlxLoggedOutSAS\r\n");
    sas_action = g_wlx_logged_out_sas(g_gina_context, WLX_SAS_TYPE_CTRL_ALT_DEL,
                                      authentication_id, logon_sid, &options,
                                      &user_token, &mpr, &profile);
    if (sas_action == 1)
        csrss_complete_logon();
    SerialPutString("[WINLOGON] WlxLoggedOutSAS returned\r\n");
    g_winlogon_thread = INVALID_HANDLE;
}

static void csrss_initialize_winlogon_session(void) {
    void *gina = PeLoadDll("MSGINA.DLL");
    uint32_t version = 0;
    int negotiated = 0;

    g_session_state = CSRSS_SESSION_BOOTING;
    g_gina_context = 0;
    if (gina) {
        SerialPutString("[WINLOGON] MSGINA loaded, resolving exports\r\n");
        g_wlx_negotiate = (WlxNegotiateFn)PeGetProcAddress(gina, "WlxNegotiate");
        SerialPutString("[WINLOGON] WlxNegotiate resolved\r\n");
        g_wlx_initialize = (WlxInitializeFn)PeGetProcAddress(gina, "WlxInitialize");
        SerialPutString("[WINLOGON] WlxInitialize resolved\r\n");
        g_wlx_logged_on_sas = (WlxLoggedOnSASFn)PeGetProcAddress(gina, "WlxLoggedOnSAS");
        SerialPutString("[WINLOGON] WlxLoggedOnSAS resolved\r\n");
        g_wlx_logged_out_sas = (WlxLoggedOutSASFn)PeGetProcAddress(gina, "WlxLoggedOutSAS");
        SerialPutString("[WINLOGON] WlxLoggedOutSAS resolved\r\n");
        g_wlx_logoff = (WlxLogoffFn)PeGetProcAddress(gina, "WlxLogoff");
        g_wlx_shutdown = (WlxShutdownFn)PeGetProcAddress(gina, "WlxShutdown");
        g_wlx_display_sas_notice = (WlxDisplaySASNoticeFn)PeGetProcAddress(gina, "WlxDisplaySASNotice");
        if (g_wlx_negotiate) negotiated = g_wlx_negotiate(WLX_VERSION_1_3, &version);
        if (negotiated && g_wlx_initialize) {
            SerialPutString("[WINLOGON] Calling WlxInitialize\r\n");
            /* The HINSTANCE passed to a GINA is its loaded module handle.
             * MSGINA uses it for LoadImage/LoadString resource lookup. */
            g_wlx_initialize(0, gina, 0, &g_wlx_dispatch, &g_gina_context);
            SerialPutString("[WINLOGON] WlxInitialize returned\r\n");
        }
    }
    SerialPutString("[WINLOGON] Session 0 initialized");
    if (negotiated) SerialPutString(" with MSGINA");
    SerialPutString("\r\n");
    /* Winlogon starts at the interactive logon desktop. The shell is not
     * created until the user submits this desktop's logon dialog. */
    g_session_state = CSRSS_SESSION_BOOTING;
    g_logon_field = 0;
    g_logon_user[0] = 0;
    g_logon_password[0] = 0;
    if (g_wlx_logged_out_sas) {
        /* MSGINA's WlxLoggedOutSAS enters a USER32 modal dialog.  It must not
         * run on this thread: this thread is also responsible for polling the
         * PS/2 controller and forwarding input to the foreground window. */
        g_winlogon_thread = KeCreateThread(csrss_winlogon_sas_thread, 0, 32768);
        if (g_winlogon_thread == INVALID_HANDLE) {
            SerialPutString("[WINLOGON] Failed to create SAS thread\r\n");
            csrss_show_logon_screen();
        }
    } else
        csrss_show_logon_screen();
}

static void csrss_queue_launch_error(const char *path, const char *detail) {
    int i = 0;

    if (path) {
        while (path[i] && i < (int)sizeof(g_pending_error_app) - 1) {
            g_pending_error_app[i] = path[i];
            i++;
        }
        g_pending_error_app[i] = 0;
    } else {
        strcpy(g_pending_error_app, "Application Error");
    }

    i = 0;
    if (detail) {
        while (detail[i] && i < (int)sizeof(g_pending_error_text) - 1) {
            g_pending_error_text[i] = detail[i];
            i++;
        }
        g_pending_error_text[i] = 0;
    } else {
        strcpy(g_pending_error_text, "The application could not be started.");
    }

    g_pending_error = 1;
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
static void csrss_fill_rect_rgb(int x, int y, int w, int h, uint32_t rgb) { FbFillRectRGB(x, y, w, h, rgb); }
static void csrss_set_color_preview(int enabled) { Win32kSetColorPreview(enabled); }
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

static void csrss_gui_thread_main(void *arg) {
    GUI_APP_THREAD_CTX *ctx = (GUI_APP_THREAD_CTX*)arg;
    uint8_t *exe_stack;
    uint32_t exe_esp;
    uint32_t saved_esp;
    int ret = 0;

    if (!ctx || !ctx->app || !ctx->entry) {
        if (ctx) kfree(ctx);
        return;
    }

    SerialPutString("[CSRSS] Standard GUI thread start pid=");
    SerialPrintDec(ctx->app->pid);
    SerialPutString(" path=");
    SerialPutString(ctx->app->path);
    SerialPutString("\r\n");

    exe_stack = (uint8_t*)kmalloc(65536);
    if (!exe_stack) {
        ctx->app->exited = 1;
        ctx->app->exit_code = -1;
        kfree(ctx);
        return;
    }

    g_current_gui_pid = ctx->app->pid;
    if (g_user32_set_process_id) g_user32_set_process_id(ctx->app->pid);
    if (g_kernel32_set_process_image_base) g_kernel32_set_process_image_base(ctx->app->image);
    exe_esp = (uint32_t)(exe_stack + 65536 - 256);
    if (ctx->kind == GUI_APP_KIND_WINMAIN) {
        GuiWinMainFn fn = (GuiWinMainFn)ctx->entry;
        SerialPutString("[CSRSS] GUI thread dispatch WinMain\r\n");
        __asm__ volatile(
            "movl %%esp, %[oldsp]\n"
            "movl %[newsp], %%esp\n"
            "push $1\n"
            "push $0\n"
            "push $0\n"
            "push %[hinst]\n"
            "call *%[fn]\n"
            "movl %%eax, %[retval]\n"
            "movl %[oldsp], %%esp\n"
            : [oldsp] "=&r"(saved_esp),
              [retval] "=m"(ret)
            : [newsp] "r"(exe_esp),
              [hinst] "g"(ctx->app->image),
              [fn] "r"(fn)
            : "eax", "ecx", "edx", "memory"
        );
    } else {
        GuiMainFn fn = (GuiMainFn)ctx->entry;
        SerialPutString("[CSRSS] GUI thread dispatch main\r\n");
        __asm__ volatile(
            "movl %%esp, %[oldsp]\n"
            "movl %[newsp], %%esp\n"
            "call *%[fn]\n"
            "movl %%eax, %[retval]\n"
            "movl %[oldsp], %%esp\n"
            : [oldsp] "=&r"(saved_esp),
              [retval] "=r"(ret)
            : [newsp] "r"(exe_esp),
              [fn] "r"(fn)
            : "eax", "ecx", "edx", "memory"
        );
    }
    g_current_gui_pid = 0;
    if (g_user32_set_process_id) g_user32_set_process_id(1);

    kfree(exe_stack);
    ctx->app->exit_code = ret;
    ctx->app->exited = 1;
    SerialPutString("[CSRSS] Standard GUI thread exit pid=");
    SerialPrintDec(ctx->app->pid);
    SerialPutString(" code=");
    SerialPrintDec((uint32_t)ret);
    SerialPutString("\r\n");
    kfree(ctx);
}

static int csrss_execute_image_sync(const char *path, uint8_t *file_buf, uint32_t file_size) {
    void *image;
    int ret;

    image = PeLoadImage(file_buf, file_size);
    if (!image) {
        csrss_queue_launch_error(path, PeGetLastError() ? PeGetLastError() : "The application image could not be mapped.");
        return -3;
    }

    if (!PeResolveImports(image)) {
        csrss_queue_launch_error(path, PeGetLastError());
        PeFreeImage(image);
        return -5;
    }
    if (!PeIsELFImage(image)) {
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
            csrss_queue_launch_error(path, "The application entry point could not be started.");
            PeFreeImage(image);
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
    return ret;
}

static int csrss_spawn_gui_instance(const char *path) {
    GUI_APP_INSTANCE *app;

    if (g_gui_app_count >= MAX_GUI_APPS) return -5;

    app = &g_gui_apps[g_gui_app_count];
    memset(app, 0, sizeof(*app));
    app->pid = g_next_gui_pid++;
    strcpy(app->path, path);
    app->window = INVALID_HANDLE;
    app->thread = INVALID_HANDLE;
    app->kind = GUI_APP_KIND_CUSTOM;
    SerialPutString("[CSRSS] Spawn GUI pid=");
    SerialPrintDec(app->pid);
    SerialPutString(" path=");
    SerialPutString(path);
    SerialPutString("\r\n");

    if (!csrss_load_gui_instance(path, app)) return -1;

    if (app->kind != GUI_APP_KIND_CUSTOM) {
        for (int tries = 0; tries < 32 && app->window == INVALID_HANDLE; tries++) {
            if (g_user32_find_top_level_window) {
                app->window = g_user32_find_top_level_window(app->pid);
                if (app->window != INVALID_HANDLE && app->window != 0) {
                    SerialPutString("[CSRSS] Bound standard app pid=");
                    SerialPrintDec(app->pid);
                    SerialPutString(" hwnd via USER32\r\n");
                    break;
                }
                app->window = INVALID_HANDLE;
            }
            KeYield();
        }
    }

    if (app->window != INVALID_HANDLE) {
        if (app->kind == GUI_APP_KIND_CUSTOM) {
            WINDOW *win = (WINDOW*)ObReferenceObject(app->window);
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

                ObDereferenceObject(app->window);
            }
        }

        Win32kActivateWindow(app->window);
        Win32kShowWindow(app->window);
    }

    g_gui_app_count++;
    Win32kRedrawAll();
    return 0;
}

static int csrss_execute_image(const char *path) {
    char upper_path[256];

    if (!path || !*path) return -1;

    uppercase_copy(upper_path, path, sizeof(upper_path));
    if (csrss_spawn_gui_instance(upper_path) == 0) {
        return 0;
    }

    strcpy(g_pending_launch_path, upper_path);
    g_pending_launch = 1;
    return 0;
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

static int csrss_set_screen_mode(int width, int height, int bpp) {
    if (!FbSetResolution(width, height, bpp)) return 0;
    for (int i = 0; i < g_gui_app_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(g_gui_apps[i].window);
        int screen_w = FbGetWidth();
        int screen_h = FbGetHeight();
        if (!win) continue;
        if (win->maximized) {
            win->x = 0; win->y = 0;
            win->width = screen_w; win->height = screen_h;
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

static void csrss_bugcheck(uint32_t code, uint32_t p1, uint32_t p2,
                           uint32_t p3, uint32_t p4) {
    KeBugCheckEx(code, p1, p2, p3, p4);
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
    csrss_fill_rect_rgb,
    csrss_set_color_preview,
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
    csrss_set_screen_mode,
    csrss_ping,
    csrss_reboot,
    csrss_shutdown,
    csrss_bugcheck
};

static int csrss_load_gui_instance(const char *path, GUI_APP_INSTANCE *app) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    GuiAppInitFn init_fn;
    GuiAppCreateMainWindowFn create_window_fn;
    GuiWinMainFn winmain_fn;
    GuiMainFn main_fn;
    GUI_APP_THREAD_CTX *thread_ctx;

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
    PeSetImagePath(app->image, path);

    if (!PeResolveImports(app->image)) {
        csrss_show_launch_error(path, PeGetLastError());
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }
    if (!PeIsELFImage(app->image)) {
        PePerformRelocations(app->image);
    }

    init_fn = (GuiAppInitFn)PeGetProcAddress(app->image, "CmdAppInit");
    create_window_fn = (GuiAppCreateMainWindowFn)PeGetProcAddress(app->image, "CmdAppCreateMainWindow");
    app->handle_key = (GuiAppHandleKeyFn)PeGetProcAddress(app->image, "CmdAppHandleKey");
    app->handle_mouse = (GuiAppHandleMouseFn)PeGetProcAddress(app->image, "CmdAppHandleMouse");
    app->should_exit = (GuiAppShouldExitFn)PeGetProcAddress(app->image, "CmdAppShouldExit");
    app->reset_exit = (GuiAppResetExitFn)PeGetProcAddress(app->image, "CmdAppResetExit");
    winmain_fn = (GuiWinMainFn)PeGetProcAddress(app->image, "WinMain");
    main_fn = (GuiMainFn)PeGetProcAddress(app->image, "main");

    if (!init_fn || !create_window_fn || !app->handle_key || !app->should_exit || !app->reset_exit) {
        if (!winmain_fn && !main_fn) {
            SerialPutString("[CSRSS] GUI app missing required exports\r\n");
            csrss_show_launch_error(path, "The application is not a valid Win32 program.");
            PeFreeImage(app->image);
            app->image = 0;
            return 0;
        }

        SerialPutString("[CSRSS] Standard app path selected pid=");
        SerialPrintDec(app->pid);
        SerialPutString(" mode=");
        SerialPutString(winmain_fn ? "WinMain" : "main");
        SerialPutString("\r\n");

        thread_ctx = (GUI_APP_THREAD_CTX*)kmalloc(sizeof(GUI_APP_THREAD_CTX));
        if (!thread_ctx) {
            PeFreeImage(app->image);
            app->image = 0;
            return 0;
        }
        memset(thread_ctx, 0, sizeof(*thread_ctx));
        thread_ctx->app = app;
        thread_ctx->kind = winmain_fn ? GUI_APP_KIND_WINMAIN : GUI_APP_KIND_MAIN;
        thread_ctx->entry = winmain_fn ? (void*)winmain_fn : (void*)main_fn;

        app->kind = thread_ctx->kind;
        app->thread = KeCreateThread(csrss_gui_thread_main, thread_ctx, 32768);
        if (app->thread == INVALID_HANDLE) {
            kfree(thread_ctx);
            PeFreeImage(app->image);
            app->image = 0;
            return 0;
        }
        return 1;
    }

    SerialPutString("[CSRSS] Custom GUI app path selected pid=");
    SerialPrintDec(app->pid);
    SerialPutString("\r\n");

    g_current_gui_pid = app->pid;
    if (!init_fn(&gui_api)) {
        SerialPutString("[CSRSS] GUI app init failed\r\n");
        g_current_gui_pid = 0;
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }

    /* A custom GUI console can provide the sink used by standard console
     * programs launched from it.  This keeps stdout attached to the owning
     * command window instead of silently dropping it in Kernel32. */
    if (g_kernel32_set_console_sink) {
        void (*sink)(const char *, uint32_t) =
            (void (*)(const char *, uint32_t))PeGetProcAddress(app->image, "CmdAppWriteConsole");
        if (sink) g_kernel32_set_console_sink(sink);
    }

    app->reset_exit();
    g_current_gui_pid = app->pid;
    app->window = create_window_fn();
    g_current_gui_pid = 0;
    if (app->window == 0xFFFFFFFFU) {
        SerialPutString("[CSRSS] GUI app failed to create main window\r\n");
        PeFreeImage(app->image);
        app->image = 0;
        return 0;
    }
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
    int8_t last_wheel = 0;
    KEYBOARD_EVENT key_event;
    int running = 1;
    HANDLE active_hwnd;

    SerialPutString("[CSRSS] Starting Win32 subsystem session\r\n");

    g_gui_app_count = 0;
    g_next_gui_pid = 1;
    g_current_gui_pid = 0;
    g_pending_error = 0;
    g_pending_launch = 0;
    g_mouse_capture = INVALID_HANDLE;
    g_lock_window = INVALID_HANDLE;
    g_logon_window = INVALID_HANDLE;
    g_session_class = INVALID_HANDLE;
    g_ctrl_down = 0;
    g_alt_down = 0;
    g_shell_started = 0;

    Win32kInit(mb_info);
    MouseInit();
    KeyboardInit();
    PeLoadDll("NTDLL.DLL");
    {
        void *kernel32_image = PeLoadDll("KERNEL32.DLL");
        if (kernel32_image) {
            g_kernel32_set_process_image_base =
                (Kernel32SetProcessImageBaseFn)PeGetProcAddress(kernel32_image, "Kernel32SetProcessImageBase");
            g_kernel32_set_console_sink =
                (Kernel32SetConsoleSinkFn)PeGetProcAddress(kernel32_image, "Kernel32SetConsoleSink");
        }
    }
    PeLoadDll("ADVAPI32.DLL");
    PeLoadDll("GDI32.DLL");
    {
        void *user32_image = PeLoadDll("USER32.DLL");
        if (user32_image) {
            g_user32_post_message = (User32PostMessageWFn)PeGetProcAddress(user32_image, "PostMessageW");
            g_user32_find_top_level_window =
                (User32FindTopLevelWindowForProcessIdFn)PeGetProcAddress(user32_image, "FindTopLevelWindowForProcessId");
            g_user32_inject_keyboard =
                (User32InjectKeyboardFn)PeGetProcAddress(user32_image, "User32InjectKeyboard");
            g_user32_inject_mouse =
                (User32InjectMouseFn)PeGetProcAddress(user32_image, "User32InjectMouse");
            g_user32_set_process_id =
                (User32SetProcessIdFn)PeGetProcAddress(user32_image, "User32SetProcessId");
        }
    }
    PeLoadDll("SHELL32.DLL");
    PeLoadDll("SHLWAPI.DLL");
    PeLoadDll("COMCTL32.DLL");
    PeLoadDll("COMDLG32.DLL");

    csrss_initialize_winlogon_session();

    MouseGetState(&mouse_state);
    last_x = mouse_state.x;
    last_y = mouse_state.y;
    last_buttons = mouse_state.buttons;
    last_wheel = mouse_state.wheel_delta;

    while (running) {
        if (g_session_state == CSRSS_SESSION_LOGGED_ON && !g_shell_started) {
            if (csrss_spawn_gui_instance("/SYSTEM32/CMD.EXE") < 0) {
                csrss_queue_launch_error("/SYSTEM32/CMD.EXE", "The logon shell could not be started.");
                g_session_state = CSRSS_SESSION_LOGGING_OFF;
            } else {
                g_shell_started = 1;
            }
        }
        while (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);

            if (status & 0x20) {
                MouseHandleByte(data);
                MouseGetState(&mouse_state);

                if (mouse_state.x != last_x || mouse_state.y != last_y) {
                    Win32kHandleMouseMove(mouse_state.x, mouse_state.y);
                    active_hwnd = g_mouse_capture != INVALID_HANDLE ?
                                  g_mouse_capture : Win32kGetActiveWindow();
                    {
                        GUI_APP_INSTANCE *app = csrss_find_app_by_window(active_hwnd);
                        if (app && app->kind == GUI_APP_KIND_CUSTOM && app->handle_mouse) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                if (!win->minimized) {
                                    g_current_gui_pid = app->pid;
                                    app->handle_mouse(mouse_state.x - client_left,
                                                      mouse_state.y - client_top,
                                                      mouse_state.buttons, GUI_MOUSE_MOVE);
                                    g_current_gui_pid = 0;
                                }
                                ObDereferenceObject(active_hwnd);
                            }
                        } else if (app && app->kind != GUI_APP_KIND_CUSTOM) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                if (!win->minimized) {
                                    if (g_user32_inject_mouse)
                                        g_user32_inject_mouse(active_hwnd, WM_MOUSEMOVE,
                                            (mouse_state.buttons & MOUSE_LEFT) ? MK_LBUTTON : 0,
                                            MAKELPARAM(mouse_state.x - client_left, mouse_state.y - client_top));
                                }
                                ObDereferenceObject(active_hwnd);
                            }
                        } else {
                            csrss_post_logon_mouse(WM_MOUSEMOVE,
                                                   (mouse_state.buttons & MOUSE_LEFT) ? MK_LBUTTON : 0,
                                                   mouse_state.x, mouse_state.y);
                        }
                    }
                    last_x = mouse_state.x;
                    last_y = mouse_state.y;
                    if (!Win32kIsDragging() && !Win32kIsResizing()) {
                        Win32kRefreshCursor();
                    }
                }

                if (mouse_state.wheel_delta != last_wheel) {
                    active_hwnd = g_mouse_capture != INVALID_HANDLE ?
                                  g_mouse_capture : Win32kGetActiveWindow();
                    {
                        GUI_APP_INSTANCE *app = csrss_find_app_by_window(active_hwnd);
                        WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                        if (app && win && !win->minimized && app->kind == GUI_APP_KIND_CUSTOM && app->handle_mouse) {
                            int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                            int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                            g_current_gui_pid = app->pid;
                            app->handle_mouse(mouse_state.x - client_left,
                                              mouse_state.y - client_top,
                                              (uint8_t)mouse_state.wheel_delta,
                                              GUI_MOUSE_WHEEL);
                            g_current_gui_pid = 0;
                        }
                        if (app && win && !win->minimized && app->kind != GUI_APP_KIND_CUSTOM) {
                            int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                            int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                            if (g_user32_inject_mouse)
                                g_user32_inject_mouse(active_hwnd, WM_MOUSEWHEEL,
                                    (uint32_t)((uint16_t)mouse_state.wheel_delta << 16),
                                    MAKELPARAM(mouse_state.x - client_left,
                                               mouse_state.y - client_top));
                        }
                        if (!app)
                            csrss_post_logon_mouse(WM_MOUSEWHEEL,
                                                   (uint32_t)mouse_state.wheel_delta,
                                                   mouse_state.x, mouse_state.y);
                        if (win) ObDereferenceObject(active_hwnd);
                    }
                    last_wheel = mouse_state.wheel_delta;
                }

                if ((mouse_state.buttons & MOUSE_LEFT) && !(last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseDown(mouse_state.x, mouse_state.y, 1);
                    active_hwnd = (g_session_state == CSRSS_SESSION_BOOTING &&
                                   g_user32_find_top_level_window) ?
                                  g_user32_find_top_level_window(1) :
                                  Win32kGetActiveWindow();
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                int client_right = win->x + win->width - 3;
                                int client_bottom = win->y + win->height - 3;

                                if (!win->minimized &&
                                    (g_mouse_capture == active_hwnd ||
                                     (mouse_state.x >= client_left && mouse_state.x < client_right &&
                                      mouse_state.y >= client_top && mouse_state.y < client_bottom))) {
                                    if (g_gui_apps[i].kind == GUI_APP_KIND_CUSTOM && g_gui_apps[i].handle_mouse) {
                                        g_current_gui_pid = g_gui_apps[i].pid;
                                        g_gui_apps[i].handle_mouse(mouse_state.x - client_left,
                                                                   mouse_state.y - client_top,
                                                                   mouse_state.buttons,
                                                                   GUI_MOUSE_LDOWN);
                                        g_current_gui_pid = 0;
                                        g_mouse_capture = active_hwnd;
                                    } else if (g_gui_apps[i].kind != GUI_APP_KIND_CUSTOM) {
                                        if (g_user32_inject_mouse)
                                            g_user32_inject_mouse(active_hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
                                                MAKELPARAM(mouse_state.x - client_left,
                                                           mouse_state.y - client_top));
                                        else
                                            csrss_post_standard_message(active_hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
                                                                        MAKELPARAM(mouse_state.x - client_left,
                                                                                   mouse_state.y - client_top));
                                    }
                                }
                                ObDereferenceObject(active_hwnd);
                            }
                            break;
                        }
                    }
                    if (!csrss_find_app_by_window(active_hwnd))
                        csrss_post_logon_mouse(WM_LBUTTONDOWN, MK_LBUTTON,
                                               mouse_state.x, mouse_state.y);
                }

                if (!(mouse_state.buttons & MOUSE_LEFT) && (last_buttons & MOUSE_LEFT)) {
                    Win32kHandleMouseUp(mouse_state.x, mouse_state.y, 1);
                    active_hwnd = g_mouse_capture != INVALID_HANDLE ?
                                  g_mouse_capture : Win32kGetActiveWindow();
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd) {
                            WINDOW *win = (WINDOW*)ObReferenceObject(active_hwnd);
                            if (win) {
                                int client_left = win->x + ((win->style & WS_CAPTION) ? 3 : 2);
                                int client_top = win->y + ((win->style & WS_CAPTION) ? 21 : 2);
                                int client_right = win->x + win->width - 3;
                                int client_bottom = win->y + win->height - 3;

                                if (!win->minimized &&
                                    mouse_state.x >= client_left && mouse_state.x < client_right &&
                                    mouse_state.y >= client_top && mouse_state.y < client_bottom) {
                                    if (g_gui_apps[i].kind == GUI_APP_KIND_CUSTOM && g_gui_apps[i].handle_mouse) {
                                        g_current_gui_pid = g_gui_apps[i].pid;
                                        g_gui_apps[i].handle_mouse(mouse_state.x - client_left,
                                                                   mouse_state.y - client_top,
                                                                   mouse_state.buttons,
                                                                   GUI_MOUSE_LUP);
                                        g_current_gui_pid = 0;
                                    } else if (g_gui_apps[i].kind != GUI_APP_KIND_CUSTOM) {
                                        if (g_user32_inject_mouse)
                                            g_user32_inject_mouse(active_hwnd, WM_LBUTTONUP, 0,
                                                MAKELPARAM(mouse_state.x - client_left,
                                                           mouse_state.y - client_top));
                                        else
                                            csrss_post_standard_message(active_hwnd, WM_LBUTTONUP, 0,
                                                                        MAKELPARAM(mouse_state.x - client_left,
                                                                                   mouse_state.y - client_top));
                                    }
                                }
                                if (g_mouse_capture == active_hwnd) g_mouse_capture = INVALID_HANDLE;
                                ObDereferenceObject(active_hwnd);
                            }
                            break;
                        }
                    }
                    if (!csrss_find_app_by_window(active_hwnd))
                        csrss_post_logon_mouse(WM_LBUTTONUP, 0,
                                               mouse_state.x, mouse_state.y);
                    Win32kRedrawAll();
                }

                last_buttons = mouse_state.buttons;
                } else {
                    KeyboardHandleData(data);
                    while (KeyboardPollEvent(&key_event)) {
                        uint32_t key_wparam = csrss_translate_key_wparam(&key_event);

                    if (g_session_state == CSRSS_SESSION_BOOTING &&
                        g_logon_window != INVALID_HANDLE) {
                        csrss_handle_logon_key(&key_event);
                        continue;
                    }

                    /* Secure attention is consumed by CSRSS/Winlogon before
                     * it reaches the foreground application. */
                    if (key_event.pressed && key_event.extended &&
                        key_event.scancode == 0x53 && key_event.ctrl && key_event.alt) {
                        csrss_handle_secure_attention();
                        continue;
                    }
                    if (g_session_state == CSRSS_SESSION_LOCKED) continue;
                    if (key_event.pressed && key_event.scancode == 0x01) running = 0;
                    active_hwnd = Win32kGetActiveWindow();
                    int delivered = 0;
                    for (int i = 0; i < g_gui_app_count; i++) {
                        if (g_gui_apps[i].window == active_hwnd) {
                            if (g_gui_apps[i].kind == GUI_APP_KIND_CUSTOM && g_gui_apps[i].handle_key) {
                                g_current_gui_pid = g_gui_apps[i].pid;
                                g_gui_apps[i].handle_key(key_event.scancode, key_event.ascii, key_event.pressed);
                                g_current_gui_pid = 0;
                            } else if (g_gui_apps[i].kind != GUI_APP_KIND_CUSTOM && key_wparam) {
                                if (g_user32_inject_keyboard)
                                    g_user32_inject_keyboard(active_hwnd, key_wparam, key_event.pressed);
                                else
                                    csrss_post_standard_message(active_hwnd,
                                                                key_event.pressed ? WM_KEYDOWN : WM_KEYUP,
                                                                key_wparam,
                                                                key_event.scancode);
                            }
                            delivered = 1;
                            break;
                        }
                    }
                    /* Winlogon/GINA and other USER32-owned windows are not
                     * represented in the application table.  Their active
                     * HWND still has a normal USER32 message queue. */
                    if (!delivered && g_session_state == CSRSS_SESSION_BOOTING &&
                        active_hwnd != INVALID_HANDLE && key_wparam)
                        if (g_user32_inject_keyboard)
                            g_user32_inject_keyboard(active_hwnd, key_wparam, key_event.pressed);
                        else
                            csrss_post_standard_message(active_hwnd,
                                                        key_event.pressed ? WM_KEYDOWN : WM_KEYUP,
                                                        key_wparam,
                                                        key_event.scancode);
                    if (key_event.pressed && active_hwnd != INVALID_HANDLE) {
                        /* Keyboard input invalidates controls in the active
                         * USER32 tree only.  Repainting the entire desktop
                         * here made Task Manager redraw all performance
                         * graphs for every typed character. */
                        Win32kUpdateWindow(active_hwnd);
                        Win32kRefreshCursor();
                    }
                }
            }
        }

        for (int i = 0; i < g_gui_app_count; ) {
            int remove = 0;

            if (g_gui_apps[i].kind == GUI_APP_KIND_CUSTOM) {
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
            } else {
                THREAD *thread = (THREAD*)ObReferenceObject(g_gui_apps[i].thread);
                if (thread) {
                    /* Wait until the scheduler has returned from the app
                     * thread before releasing its code image.  The app sets
                     * exited just before returning, so using that flag here
                     * can free executable memory while it is still on the
                     * thread's call path. */
                    if (thread->state == THREAD_TERMINATED) {
                        if (g_gui_apps[i].window != INVALID_HANDLE) {
                            /* A standard WinMain app is allowed to return
                             * without destroying its last window.  CSRSS
                             * owns the process lifetime, so tear the window
                             * down here before releasing the image. */
                            if (ObReferenceObject(g_gui_apps[i].window)) {
                                ObDereferenceObject(g_gui_apps[i].window);
                                Win32kDestroyWindow(g_gui_apps[i].window);
                            }
                        }
                        remove = 1;
                    }
                    ObDereferenceObject(g_gui_apps[i].thread);
                }
            }

            if (remove) {
                if (g_kernel32_set_console_sink &&
                    g_gui_apps[i].kind == GUI_APP_KIND_CUSTOM &&
                    strcmp(g_gui_apps[i].path, "/SYSTEM32/CMD.EXE") == 0)
                    g_kernel32_set_console_sink(0);
                if (g_gui_apps[i].image) {
                    PeFreeImage(g_gui_apps[i].image);
                    g_gui_apps[i].image = 0;
                }
                if (g_gui_apps[i].kind != GUI_APP_KIND_CUSTOM &&
                    g_gui_apps[i].thread != INVALID_HANDLE) {
                    /* Release the owning handle reference.  The temporary
                     * reference above was already dropped. */
                    ObDereferenceObject(g_gui_apps[i].thread);
                    g_gui_apps[i].thread = INVALID_HANDLE;
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

        if (g_gui_app_count == 0 && g_session_state == CSRSS_SESSION_LOGGING_OFF)
            running = 0;

        if (g_pending_launch) {
            uint8_t *file_buf = 0;
            uint32_t file_size = 0;
            char launch_path[256];
            g_pending_launch = 0;
            strcpy(launch_path, g_pending_launch_path);

            if (!CdfsReadFile(launch_path, &file_buf, &file_size)) {
                csrss_queue_launch_error(launch_path, "The system cannot find the file specified.");
            } else if (((file_size < 64 || file_buf[0] != 0x4D || file_buf[1] != 0x5A) &&
                        (file_size < 4 || *(uint32_t*)file_buf != 0x464C457F))) {
                csrss_queue_launch_error(launch_path, "The application is not a valid Win32 program.");
                kfree(file_buf);
            } else {
                csrss_execute_image_sync(launch_path, file_buf, file_size);
                kfree(file_buf);
            }
        }

        if (g_pending_error) {
            g_pending_error = 0;
            csrss_show_launch_error(g_pending_error_app, g_pending_error_text);
        }

        for (volatile int i = 0; i < 3000; i++);
        /* Let the Winlogon/USER32 modal thread and standard GUI threads run.
         * Without an explicit yield this polling loop monopolizes the
         * cooperative scheduler and the logon dialog appears frozen. */
        KeYield();
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

    if (g_logon_window != INVALID_HANDLE) {
        Win32kDestroyWindow(g_logon_window);
        g_logon_window = INVALID_HANDLE;
    }
    if (g_lock_window != INVALID_HANDLE) csrss_hide_lock_screen();
    if (g_session_state == CSRSS_SESSION_LOGGED_ON && g_wlx_logoff) {
        g_session_state = CSRSS_SESSION_LOGGING_OFF;
        g_wlx_logoff(g_gina_context);
    }
    if (g_wlx_shutdown) {
        g_session_state = CSRSS_SESSION_SHUTTING_DOWN;
        g_wlx_shutdown(g_gina_context, WLX_SHUTDOWN_LOGOFF);
    }

    restore_text_mode();
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString(DISCOUNT_NAME "\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Returned from CSRSS session.\n\n", 0x0A);
}
