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

#define MAX_MODES 16
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
static int selected_mode = 0;
static int applied_mode = 0;

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

static void load_modes(void) {
    int count;
    g_mode_count = 0;
    memset(g_modes, 0, sizeof(g_modes));

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
}

static int apply_selected_mode(void) {
    if (!g_api || !g_api->SetScreenResolution) return 0;
    if (selected_mode < 0 || selected_mode >= g_mode_count) return 0;
    if (g_api->SetScreenResolution(g_modes[selected_mode].width, g_modes[selected_mode].height)) {
        applied_mode = selected_mode;
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
        int slider_x;
        int slider_y;
        int slider_w;
        char res_text[64];
        char num[16];

        g_api->GetClientRect(hwnd, &client);
        g_api->GetWindowRect(hwnd, &win);
        x0 = win.left + client.left;
        y0 = win.top + client.top;
        cw = client.right - client.left;
        ch = client.bottom - client.top;

        g_api->FillRect(x0, y0, cw, ch, COLOR_LIGHT_GRAY);

        g_api->FillRect(x0 + 16, y0 + 16, 132, 102, COLOR_DARK_GRAY);
        g_api->DrawRect(x0 + 16, y0 + 16, 132, 102, COLOR_WHITE);
        g_api->FillRect(x0 + 28, y0 + 28, 108, 76, COLOR_LIGHT_BLUE);
        g_api->DrawRect(x0 + 28, y0 + 28, 108, 76, COLOR_WHITE);
        g_api->FillRect(x0 + 56, y0 + 48, 52, 32, COLOR_BLUE);
        g_api->DrawRect(x0 + 56, y0 + 48, 52, 32, COLOR_WHITE);
        itoa(g_modes[selected_mode].width, num, 10);
        g_api->DrawString(x0 + 58, y0 + 58, num, COLOR_WHITE, COLOR_BLUE);

        g_api->DrawString(x0 + 168, y0 + 18, "Display", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 168, y0 + 34, "Desktop area", COLOR_BLACK, COLOR_LIGHT_GRAY);

        slider_x = x0 + 168;
        slider_y = y0 + 54;
        slider_w = 176;

        g_api->FillRect(slider_x, slider_y + 6, slider_w, 4, COLOR_DARK_GRAY);
        if (g_mode_count <= 1) {
            g_api->FillRect(slider_x, slider_y, 10, 16, COLOR_WHITE);
            g_api->DrawRect(slider_x, slider_y, 10, 16, COLOR_BLUE);
        } else {
            for (int i = 0; i < g_mode_count; i++) {
                int px = slider_x + (i * (slider_w - 8)) / (g_mode_count - 1);
                g_api->FillRect(px, slider_y + 2, 2, 12, COLOR_BLACK);
            }
            {
                int knob_x = slider_x + (selected_mode * (slider_w - 8)) / (g_mode_count - 1);
                g_api->FillRect(knob_x, slider_y, 10, 16, COLOR_WHITE);
                g_api->DrawRect(knob_x, slider_y, 10, 16, COLOR_BLUE);
            }
        }

        res_text[0] = 0;
        mode_to_text(selected_mode, res_text);
        g_api->DrawString(x0 + 168, y0 + 78, res_text, COLOR_BLACK, COLOR_LIGHT_GRAY);

        g_api->DrawString(x0 + 16, y0 + 132, "Available resolutions", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->FillRect(x0 + 16, y0 + 146, 240, 126, COLOR_WHITE);
        g_api->DrawRect(x0 + 16, y0 + 146, 240, 126, COLOR_DARK_GRAY);

        for (int i = 0; i < g_mode_count; i++) {
            int iy = y0 + 150 + (i * 14);
            uint8_t bg = (i == selected_mode) ? COLOR_LIGHT_BLUE : COLOR_WHITE;
            uint8_t fg = (i == selected_mode) ? COLOR_WHITE : COLOR_BLACK;
            char line[48];
            line[0] = 0;
            mode_to_text(i, line);
            g_api->FillRect(x0 + 18, iy, 236, 12, bg);
            g_api->DrawString(x0 + 22, iy + 2, line, fg, bg);
        }

        g_api->DrawString(x0 + 280, y0 + 150, "Monitor:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 280, y0 + 164, "Generic QEMU Display", COLOR_BLACK, COLOR_LIGHT_GRAY);
        g_api->DrawString(x0 + 280, y0 + 188, "Current mode:", COLOR_BLACK, COLOR_LIGHT_GRAY);
        res_text[0] = 0;
        mode_to_text(applied_mode, res_text);
        g_api->DrawString(x0 + 280, y0 + 202, res_text, COLOR_BLUE, COLOR_LIGHT_GRAY);

        draw_button(x0 + cw - 220, y0 + ch - 34, 60, 24, "Apply", selected_mode != applied_mode);
        draw_button(x0 + cw - 152, y0 + ch - 34, 60, 24, "OK", 0);
        draw_button(x0 + cw - 84, y0 + ch - 34, 68, 24, "Cancel", 0);
    }
}

__attribute__((visibility("default"))) int CmdAppInit(const GUI_APP_API *api) {
    int screen_w;
    int screen_h;

    g_api = api;
    desk_class = 0xFFFFFFFFU;
    desk_window = 0xFFFFFFFFU;
    desk_exit_requested = 0;

    load_modes();
    if (g_mode_count <= 0) return 0;

    screen_w = (g_api && g_api->GetScreenWidth) ? g_api->GetScreenWidth() : g_modes[0].width;
    screen_h = (g_api && g_api->GetScreenHeight) ? g_api->GetScreenHeight() : g_modes[0].height;
    selected_mode = find_mode(screen_w, screen_h, 32);
    applied_mode = selected_mode;
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
    if (!pressed || g_mode_count <= 0) return;

    switch (scancode) {
        case 0x01:
            desk_exit_requested = 1;
            return;
        case 0x4B:
        case 0x48:
            if (selected_mode > 0) selected_mode--;
            return;
        case 0x4D:
        case 0x50:
            if (selected_mode + 1 < g_mode_count) selected_mode++;
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
    int slider_x = 168;
    int slider_y = 54;
    int slider_w = 176;
    int list_x = 16;
    int list_y = 146;
    int list_w = 240;
    int list_h = 126;
    int client_w = DESK_WINDOW_W - 6;
    int client_h = DESK_WINDOW_H - 24;
    int apply_x = client_w - 220;
    int ok_x = client_w - 152;
    int cancel_x = client_w - 84;
    int btn_y = client_h - 34;

    if (event_type != GUI_MOUSE_LDOWN || !(buttons & 1) || g_mode_count <= 0) return;

    if (x >= slider_x && x < slider_x + slider_w && y >= slider_y && y < slider_y + 20) {
        int idx;
        if (g_mode_count <= 1) idx = 0;
        else {
            int pos = x - slider_x;
            idx = (pos * (g_mode_count - 1) + ((slider_w - 8) / 2)) / (slider_w - 8);
        }
        if (idx < 0) idx = 0;
        if (idx >= g_mode_count) idx = g_mode_count - 1;
        selected_mode = idx;
        return;
    }

    if (x >= list_x && x < list_x + list_w && y >= list_y && y < list_y + list_h) {
        int item = (y - (list_y + 4)) / 14;
        if (item >= 0 && item < g_mode_count) selected_mode = item;
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
