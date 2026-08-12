#include <stdint.h>
#include "guiapp.h"

#define COLOR_BLACK       0
#define COLOR_BLUE        1
#define COLOR_CYAN        3
#define COLOR_RED         4
#define COLOR_LIGHT_GRAY  7
#define COLOR_DARK_GRAY   8
#define COLOR_LIGHT_BLUE  9
#define COLOR_WHITE       15

#define MAX_MODES 48
#define DESK_WINDOW_W 520
#define DESK_WINDOW_H 340

typedef struct _RES_MODE {
    int width;
    int height;
    int bpp;
} RES_MODE;

static const GUI_APP_API *g_api = 0;
static GUI_HANDLE desk_class = 0xFFFFFFFFU;
static GUI_HANDLE desk_window = 0xFFFFFFFFU;
static int desk_exit_requested = 0;
static RES_MODE g_modes[MAX_MODES];
static int g_mode_count = 0;
static RES_MODE g_resolutions[MAX_MODES];
static int g_resolution_count = 0;
static int g_depths[4];
static int g_depth_count = 0;
static int selected_resolution = 0;
static int applied_resolution = 0;
static int selected_depth = 32;
static int applied_depth = 32;
static int resolution_open = 0;
static int depth_open = 0;
static int color_preview = 0;

extern void *memset(void *s, int c, uint32_t n);
extern void strcat(char *d, const char *s);
extern void itoa(int value, char *str, int base);

static void mode_to_text(int index, char *buf) {
    char num[16];
    buf[0] = 0;
    itoa(g_modes[index].width, num, 10);
    strcat(buf, num);
    strcat(buf, " by ");
    itoa(g_modes[index].height, num, 10);
    strcat(buf, num);
    strcat(buf, " pixels");
}

static int find_mode(int width, int height, int bpp) {
    for (int i = 0; i < g_mode_count; i++) {
        if (g_modes[i].width == width &&
            g_modes[i].height == height &&
            g_modes[i].bpp == bpp) return i;
    }
    return 0;
}

static int find_resolution(int width, int height) {
    for (int i = 0; i < g_resolution_count; i++)
        if (g_resolutions[i].width == width && g_resolutions[i].height == height) return i;
    return 0;
}

static int has_depth(int bpp) {
    for (int i = 0; i < g_depth_count; i++) if (g_depths[i] == bpp) return 1;
    return 0;
}

static int depth_available_for_resolution(int bpp, int resolution) {
    if (resolution < 0 || resolution >= g_resolution_count) return 0;
    for (int i = 0; i < g_mode_count; i++)
        if (g_modes[i].width == g_resolutions[resolution].width &&
            g_modes[i].height == g_resolutions[resolution].height &&
            g_modes[i].bpp == bpp) return 1;
    return 0;
}

static int mode_index(void) {
    if (selected_resolution < 0 || selected_resolution >= g_resolution_count) return 0;
    for (int i = 0; i < g_mode_count; i++)
        if (g_modes[i].width == g_resolutions[selected_resolution].width &&
            g_modes[i].height == g_resolutions[selected_resolution].height &&
            g_modes[i].bpp == selected_depth) return i;
    return find_mode(g_resolutions[selected_resolution].width,
                     g_resolutions[selected_resolution].height, 32);
}

static void load_modes(void) {
    int count;
    g_mode_count = 0;
    g_resolution_count = 0;
    g_depth_count = 0;
    memset(g_modes, 0, sizeof(g_modes));
    memset(g_resolutions, 0, sizeof(g_resolutions));
    memset(g_depths, 0, sizeof(g_depths));

    if (!g_api || !g_api->GetScreenModeCount || !g_api->GetScreenModeInfo) return;

    count = g_api->GetScreenModeCount();
    if (count > MAX_MODES) count = MAX_MODES;

    for (int i = 0; i < count; i++) {
        if (g_api->GetScreenModeInfo(i, &g_modes[g_mode_count].width,
                                     &g_modes[g_mode_count].height,
                                     &g_modes[g_mode_count].bpp)) {
            g_mode_count++;
        }
    }
    for (int i = 0; i < g_mode_count; i++) {
        int exists = 0;
        for (int j = 0; j < g_resolution_count; j++)
            if (g_resolutions[j].width == g_modes[i].width && g_resolutions[j].height == g_modes[i].height) exists = 1;
        if (!exists && g_resolution_count < MAX_MODES) g_resolutions[g_resolution_count++] = g_modes[i];
        if (!has_depth(g_modes[i].bpp) && g_depth_count < 4) g_depths[g_depth_count++] = g_modes[i].bpp;
    }
}

static int apply_selected_mode(void) {
    if (!g_api || !g_api->SetScreenResolution) return 0;
    if (selected_resolution < 0 || selected_resolution >= g_resolution_count) return 0;
    if (g_api->SetScreenMode && g_api->SetScreenMode(g_resolutions[selected_resolution].width,
                                                     g_resolutions[selected_resolution].height,
                                                     selected_depth)) {
        applied_resolution = selected_resolution;
        applied_depth = selected_depth;
        return 1;
    }
    if (g_api->SetScreenResolution(g_resolutions[selected_resolution].width,
                                   g_resolutions[selected_resolution].height)) {
        applied_resolution = selected_resolution;
        applied_depth = 32;
        return 1;
    }
    return 0;
}

static void draw_button(int x, int y, int w, int h, const char *label, int active) {
    uint8_t face = active ? COLOR_LIGHT_BLUE : COLOR_LIGHT_GRAY;
    uint8_t text = active ? COLOR_WHITE : COLOR_BLACK;
    g_api->FillRect(x, y, w, h, face);
    g_api->DrawRect(x, y, w, h, COLOR_DARK_GRAY);
    g_api->DrawString(x + 10, y + 8, label, text, face);
}

static void desk_wndproc(GUI_HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == GUI_WM_PAINT && g_api) {
        GUI_RECT client;
        GUI_RECT win;
        int x0;
        int y0;
        int cw;
        int ch;
        int win_w;
        int win_h;
        int border_x;
        int border_y;
        int mode_idx;
        char res_text[64];
        char num[16];

        g_api->GetClientRect(hwnd, &client);
        if (color_preview) {
            int sw = g_api->GetScreenWidth ? g_api->GetScreenWidth() : 640;
            int sh = g_api->GetScreenHeight ? g_api->GetScreenHeight() : 480;
            return;
        }
        g_api->GetWindowRect(hwnd, &win);
        cw = client.right - client.left;
        ch = client.bottom - client.top;
        win_w = win.right - win.left;
        win_h = win.bottom - win.top;
        border_x = (win_w > cw) ? ((win_w - cw) / 2) : 0;
        border_y = (win_h > ch) ? (win_h - ch - border_x) : 0;
        x0 = win.left + border_x;
        y0 = win.top + border_y;

        g_api->FillRect(x0, y0, cw, ch, COLOR_LIGHT_GRAY);

        /* ReactOS-style two comboboxes: resolution first, depth second. */
        g_api->DrawString(x0 + 22, y0 + 26, "Resolution:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->FillRect(x0 + 140, y0 + 20, 260, 22, COLOR_WHITE);
        g_api->DrawRect(x0 + 140, y0 + 20, 260, 22, COLOR_DARK_GRAY);
        mode_to_text(mode_idx = mode_index(), res_text);
        g_api->DrawString(x0 + 148, y0 + 27, res_text, COLOR_BLACK, COLOR_WHITE);
        g_api->FillRect(x0 + 378, y0 + 21, 21, 20, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 385, y0 + 26, "v", COLOR_BLACK, COLOR_LIGHT_GRAY);
        if (resolution_open) for (int i = 0; i < g_resolution_count && i < 16; i++) {
            char line[64];
            RES_MODE saved = g_modes[0];
            g_modes[0] = g_resolutions[i]; mode_to_text(0, line); g_modes[0] = saved;
            g_api->FillRect(x0 + 140, y0 + 42 + i * 14, 260, 14, i == selected_resolution ? COLOR_LIGHT_BLUE : COLOR_WHITE);
            g_api->DrawString(x0 + 148, y0 + 44 + i * 14, line, i == selected_resolution ? COLOR_WHITE : COLOR_BLACK, i == selected_resolution ? COLOR_LIGHT_BLUE : COLOR_WHITE);
        }
        g_api->DrawString(x0 + 22, y0 + 72, "Color depth:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->FillRect(x0 + 140, y0 + 66, 260, 22, COLOR_WHITE);
        g_api->DrawRect(x0 + 140, y0 + 66, 260, 22, COLOR_DARK_GRAY);
        itoa(selected_depth, num, 10);
        g_api->DrawString(x0 + 148, y0 + 73, num, COLOR_BLACK, COLOR_WHITE);
        g_api->DrawString(x0 + 172, y0 + 73, "bits", COLOR_BLACK, COLOR_WHITE);
        g_api->FillRect(x0 + 378, y0 + 67, 21, 20, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 385, y0 + 72, "v", COLOR_BLACK, COLOR_LIGHT_GRAY);
        if (depth_open) for (int i = 0, shown = 0; i < g_depth_count; i++) {
            if (!depth_available_for_resolution(g_depths[i], selected_resolution)) continue;
            itoa(g_depths[i], num, 10);
            g_api->FillRect(x0 + 140, y0 + 88 + shown * 14, 260, 14, g_depths[i] == selected_depth ? COLOR_LIGHT_BLUE : COLOR_WHITE);
            g_api->DrawString(x0 + 148, y0 + 90 + shown * 14, num, g_depths[i] == selected_depth ? COLOR_WHITE : COLOR_BLACK, g_depths[i] == selected_depth ? COLOR_LIGHT_BLUE : COLOR_WHITE);
            shown++;
        }

        g_api->DrawString(x0 + 22, y0 + 120, "Monitor:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 100, y0 + 120, "Generic Display", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 22, y0 + 140, "Current mode:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        res_text[0] = 0;
        mode_to_text(find_mode(g_resolutions[applied_resolution].width, g_resolutions[applied_resolution].height, applied_depth), res_text);
        g_api->DrawString(x0 + 130, y0 + 140, res_text, COLOR_BLUE, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 22, y0 + 160, "Color depth:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        itoa(applied_depth, num, 10);
        g_api->DrawString(x0 + 130, y0 + 160, num, COLOR_BLUE, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 152, y0 + 160, "bits", COLOR_BLUE, COLOR_LIGHT_GRAY);

        draw_button(x0 + cw - 220, y0 + ch - 34, 60, 24, "Apply", selected_resolution != applied_resolution || selected_depth != applied_depth);
        draw_button(x0 + cw - 152, y0 + ch - 34, 60, 24, "OK", 0);
        draw_button(x0 + cw - 84, y0 + ch - 34, 68, 24, "Cancel", 0);
        draw_button(x0 + 16, y0 + ch - 34, 128, 24, "Color preview", 0);

        /* Popup lists are painted last, like real combo boxes.  Otherwise
         * the controls underneath (especially Color depth) overwrite the
         * lower half of an open resolution list. */
        if (resolution_open) for (int i = 0; i < g_resolution_count && i < 16; i++) {
            char line[64];
            RES_MODE saved = g_modes[0];
            g_modes[0] = g_resolutions[i]; mode_to_text(0, line); g_modes[0] = saved;
            g_api->FillRect(x0 + 140, y0 + 42 + i * 14, 260, 14, i == selected_resolution ? COLOR_LIGHT_BLUE : COLOR_WHITE);
            g_api->DrawString(x0 + 148, y0 + 44 + i * 14, line, i == selected_resolution ? COLOR_WHITE : COLOR_BLACK, i == selected_resolution ? COLOR_LIGHT_BLUE : COLOR_WHITE);
        }
        if (depth_open) {
            int shown = 0;
            for (int i = 0; i < g_depth_count; i++) {
                if (!depth_available_for_resolution(g_depths[i], selected_resolution)) continue;
                itoa(g_depths[i], num, 10);
                g_api->FillRect(x0 + 140, y0 + 88 + shown * 14, 260, 14, g_depths[i] == selected_depth ? COLOR_LIGHT_BLUE : COLOR_WHITE);
                g_api->DrawString(x0 + 148, y0 + 90 + shown * 14, num, g_depths[i] == selected_depth ? COLOR_WHITE : COLOR_BLACK, g_depths[i] == selected_depth ? COLOR_LIGHT_BLUE : COLOR_WHITE);
                shown++;
            }
        }
    }
}

__attribute__((visibility("default"))) int CmdAppInit(const GUI_APP_API *api) {
    int screen_w;
    int screen_h;

    g_api = api;
    desk_class = 0xFFFFFFFFU;
    desk_window = 0xFFFFFFFFU;
    desk_exit_requested = 0;
    color_preview = 0;

    load_modes();
    if (g_mode_count <= 0) return 0;

    screen_w = (g_api && g_api->GetScreenWidth) ? g_api->GetScreenWidth() : g_modes[0].width;
    screen_h = (g_api && g_api->GetScreenHeight) ? g_api->GetScreenHeight() : g_modes[0].height;
    selected_resolution = find_resolution(screen_w, screen_h);
    applied_resolution = selected_resolution;
    selected_depth = 32;
    for (int i = 0; i < g_mode_count; i++)
        if (g_modes[i].width == screen_w && g_modes[i].height == screen_h) selected_depth = g_modes[i].bpp;
    applied_depth = selected_depth;
    return 1;
}

__attribute__((visibility("default"))) GUI_HANDLE CmdAppCreateMainWindow(void) {
    if (!g_api) return 0xFFFFFFFFU;
    if (desk_class == 0xFFFFFFFFU) {
        desk_class = g_api->RegisterClass("DeskCplClass", 0, desk_wndproc);
    }
    desk_window = g_api->CreateWindowByClass(desk_class, "Display Properties",
                                             108, 72, DESK_WINDOW_W, DESK_WINDOW_H, GUI_WS_OVERLAPPEDWINDOW);
    return desk_window;
}

__attribute__((visibility("default"))) void CmdAppHandleKey(uint8_t scancode, char ascii, uint8_t pressed) {
    (void)ascii;
    if (!pressed) return;
    if (color_preview) {
        color_preview = 0;
        if (g_api && g_api->SetColorPreview) g_api->SetColorPreview(0);
        if (g_api && g_api->UpdateWindow) g_api->UpdateWindow(desk_window);
        return;
    }
    if (g_resolution_count <= 0) return;

    switch (scancode) {
        case 0x01:
            desk_exit_requested = 1;
            return;
        case 0x4B:
        case 0x48:
            if (selected_resolution > 0) selected_resolution--;
            return;
        case 0x4D:
        case 0x50:
            if (selected_resolution + 1 < g_resolution_count) selected_resolution++;
            return;
        case 0x1C:
            apply_selected_mode();
            desk_exit_requested = 1;
            return;
        case 0x39:
            apply_selected_mode();
            return;
    }
}

__attribute__((visibility("default"))) void CmdAppHandleMouse(int x, int y, uint8_t buttons, uint8_t event_type) {
    int client_w = DESK_WINDOW_W - 6;
    int client_h = DESK_WINDOW_H - 24;
    int apply_x = client_w - 220;
    int ok_x = client_w - 152;
    int cancel_x = client_w - 84;
    int btn_y = client_h - 34;
    int preview_x = 16;

    if (event_type != GUI_MOUSE_LDOWN || !(buttons & 1) || g_resolution_count <= 0) return;

    if (x >= preview_x && x < preview_x + 128 && y >= btn_y && y < btn_y + 24) {
        color_preview = 1;
        if (g_api && g_api->SetColorPreview) g_api->SetColorPreview(1);
        return;
    }

    if (x >= 140 && x < 400 && y >= 20 && y < 42) {
        resolution_open = !resolution_open; depth_open = 0;
        return;
    }
    if (resolution_open && x >= 140 && x < 400 && y >= 42 && y < 42 + g_resolution_count * 14) {
        int item = (y - 42) / 14;
        if (item >= 0 && item < g_resolution_count) {
            selected_resolution = item;
            if (!depth_available_for_resolution(selected_depth, selected_resolution)) {
                for (int i = 0; i < g_depth_count; i++)
                    if (depth_available_for_resolution(g_depths[i], selected_resolution)) { selected_depth = g_depths[i]; break; }
            }
        }
        resolution_open = 0;
        return;
    }
    if (x >= 140 && x < 400 && y >= 66 && y < 88) {
        depth_open = !depth_open; resolution_open = 0;
        return;
    }
    if (depth_open && x >= 140 && x < 400 && y >= 88 && y < 88 + g_depth_count * 14) {
        int item = (y - 88) / 14;
        int shown = 0;
        for (int i = 0; i < g_depth_count; i++) {
            if (!depth_available_for_resolution(g_depths[i], selected_resolution)) continue;
            if (shown++ == item) { selected_depth = g_depths[i]; break; }
        }
        depth_open = 0;
        return;
    }

    if (y >= btn_y && y < btn_y + 24) {
        if (x >= apply_x && x < apply_x + 60) {
            apply_selected_mode();
            return;
        }
        if (x >= ok_x && x < ok_x + 60) {
            apply_selected_mode();
            desk_exit_requested = 1;
            return;
        }
        if (x >= cancel_x && x < cancel_x + 68) {
            desk_exit_requested = 1;
            return;
        }
    }
}

__attribute__((visibility("default"))) int CmdAppShouldExit(void) {
    return desk_exit_requested;
}

__attribute__((visibility("default"))) void CmdAppResetExit(void) {
    desk_exit_requested = 0;
}

int main(void) {
    return 0;
}
