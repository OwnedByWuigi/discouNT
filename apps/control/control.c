#include <stdint.h>
#include "guiapp.h"

#define COLOR_BLACK       0
#define COLOR_BLUE        1
#define COLOR_CYAN        3
#define COLOR_RED         4
#define COLOR_LIGHT_GRAY  7
#define COLOR_DARK_GRAY   8
#define COLOR_LIGHT_BLUE  9
#define COLOR_YELLOW      14
#define COLOR_WHITE       15

#define MAX_CPLS 16

typedef struct _CPL_ITEM {
    char name[32];
    char path[64];
} CPL_ITEM;

static const GUI_APP_API *g_api = 0;
static GUI_HANDLE control_class = 0xFFFFFFFFU;
static GUI_HANDLE control_window = 0xFFFFFFFFU;
static int control_exit_requested = 0;
static CPL_ITEM g_items[MAX_CPLS];
static int g_item_count = 0;
static int g_selected = 0;

extern void *memset(void *s, int c, uint32_t n);
extern uint32_t strlen(const char *s);
extern int strcmp(const char *a, const char *b);
extern void strcpy(char *d, const char *s);
extern void strcat(char *d, const char *s);

static int ends_with_cpl(const char *name) {
    int len = (int)strlen(name);
    if (len < 4) return 0;
    return name[len - 4] == '.' &&
           name[len - 3] == 'C' &&
           name[len - 2] == 'P' &&
           name[len - 1] == 'L';
}

static int sector_find_entry(uint32_t dir_lba, uint32_t dir_size, const char *name,
                             uint32_t *out_lba, uint32_t *out_size, int *is_dir) {
    uint8_t sector[2048];
    uint32_t sectors = (dir_size + 2047) / 2048;

    for (uint32_t s = 0; s < sectors; s++) {
        int off = 0;
        if (!g_api || !g_api->ReadSector || !g_api->ReadSector(dir_lba + s, sector)) return 0;
        while (off < 2048) {
            uint8_t len = sector[off];
            if (len == 0) break;
            {
                uint8_t flags = sector[off + 25];
                uint8_t name_len = sector[off + 32];
                char entry[256];
                int ei = 0;
                char *dname = (char*)(sector + off + 33);
                for (int i = 0; i < name_len && ei < 255; i++) {
                    if (dname[i] == ';') break;
                    entry[ei++] = dname[i];
                }
                entry[ei] = 0;
                if (strcmp(name, entry) == 0) {
                    *out_lba = *(uint32_t*)(sector + off + 2);
                    *out_size = *(uint32_t*)(sector + off + 10);
                    *is_dir = (flags & 0x02) ? 1 : 0;
                    return 1;
                }
            }
            off += len;
        }
    }
    return 0;
}

static int resolve_system32(uint32_t *out_lba, uint32_t *out_size) {
    uint8_t sector[2048];
    uint32_t root_lba;
    uint32_t root_size;
    int is_dir;

    if (!g_api || !g_api->ReadSector || !g_api->ReadSector(16, sector)) return 0;
    root_lba = *(uint32_t*)(sector + 156 + 2);
    root_size = *(uint32_t*)(sector + 156 + 10);
    if (!sector_find_entry(root_lba, root_size, "DISCOUNT", out_lba, out_size, &is_dir) || !is_dir) return 0;
    return sector_find_entry(*out_lba, *out_size, "SYSTEM32", out_lba, out_size, &is_dir) && is_dir;
}

static void load_cpls(void) {
    uint8_t sector[2048];
    uint32_t dir_lba;
    uint32_t dir_size;
    uint32_t sectors;

    g_item_count = 0;
    g_selected = 0;

    if (!resolve_system32(&dir_lba, &dir_size)) return;

    sectors = (dir_size + 2047) / 2048;
    for (uint32_t s = 0; s < sectors && g_item_count < MAX_CPLS; s++) {
        int off = 0;
        if (!g_api->ReadSector(dir_lba + s, sector)) break;
        while (off < 2048 && g_item_count < MAX_CPLS) {
            uint8_t len = sector[off];
            if (len == 0) break;
            {
                uint8_t flags = sector[off + 25];
                uint8_t name_len = sector[off + 32];
                char entry[64];
                int ei = 0;
                char *dname = (char*)(sector + off + 33);

                for (int i = 0; i < name_len && ei < 63; i++) {
                    if (dname[i] == ';') break;
                    entry[ei++] = dname[i];
                }
                entry[ei] = 0;

                if (!(flags & 0x02) && ends_with_cpl(entry)) {
                    strcpy(g_items[g_item_count].name, entry);
                    strcpy(g_items[g_item_count].path, "/DISCOUNT/SYSTEM32/");
                    strcat(g_items[g_item_count].path, entry);
                    g_item_count++;
                }
            }
            off += len;
        }
    }
}

static void launch_selected(void) {
    if (!g_api || !g_api->ExecuteImage) return;
    if (g_selected < 0 || g_selected >= g_item_count) return;
    g_api->ExecuteImage(g_items[g_selected].path);
}

static int grid_columns(int client_w) {
    if (client_w >= 360) return 3;
    if (client_w >= 240) return 2;
    return 1;
}

static void draw_item(int x, int y, int w, int h, const CPL_ITEM *item, int selected) {
    uint8_t face = selected ? COLOR_LIGHT_BLUE : COLOR_LIGHT_GRAY;
    uint8_t text = selected ? COLOR_WHITE : COLOR_BLACK;

    g_api->FillRect(x, y, w, h, face);
    g_api->DrawRect(x, y, w, h, selected ? COLOR_WHITE : COLOR_DARK_GRAY);

    g_api->FillRect(x + 12, y + 10, 32, 24, COLOR_BLUE);
    g_api->DrawRect(x + 12, y + 10, 32, 24, COLOR_WHITE);
    g_api->FillRect(x + 16, y + 16, 24, 4, COLOR_CYAN);
    g_api->FillRect(x + 22, y + 22, 12, 6, COLOR_YELLOW);

    g_api->DrawString(x + 56, y + 20, item->name, text, face);
}

static void control_wndproc(GUI_HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == GUI_WM_CREATE) {
        load_cpls();
        return;
    }

    if (msg == GUI_WM_PAINT && g_api) {
        GUI_RECT client;
        GUI_RECT win;
        int client_x;
        int client_y;
        int client_w;
        int client_h;
        int win_w;
        int win_h;
        int border_x;
        int border_y;
        int cols;
        int cell_w;
        int cell_h = 64;

        g_api->GetClientRect(hwnd, &client);
        g_api->GetWindowRect(hwnd, &win);

        client_w = client.right - client.left;
        client_h = client.bottom - client.top;
        win_w = win.right - win.left;
        win_h = win.bottom - win.top;

        /* GetClientRect is client-relative (0,0), while the GUI drawing
         * API takes screen coordinates.  Account for the non-client frame;
         * otherwise the panel is painted through the title bar. */
        border_x = (win_w > client_w) ? ((win_w - client_w) / 2) : 0;
        border_y = (win_h > client_h) ? (win_h - client_h - border_x) : 0;
        client_x = win.left + border_x;
        client_y = win.top + border_y;

        g_api->FillRect(client_x, client_y, client_w, client_h, COLOR_LIGHT_GRAY);
        g_api->DrawString(client_x + 8, client_y + 8,
                          "Control Panel", COLOR_BLACK, COLOR_LIGHT_GRAY);

        cols = grid_columns(client_w - 16);
        cell_w = (client_w - 16 - ((cols - 1) * 8)) / cols;
        if (cell_w < 120) cell_w = 120;

        for (int i = 0; i < g_item_count; i++) {
            int row = i / cols;
            int col = i % cols;
            int x = client_x + 8 + col * (cell_w + 8);
            int y = client_y + 28 + row * (cell_h + 8);
            draw_item(x, y, cell_w, cell_h, &g_items[i], i == g_selected);
        }

        if (g_item_count == 0) {
            g_api->DrawString(client_x + 8, client_y + 36,
                              "No .CPL files were found in SYSTEM32.",
                              COLOR_RED, COLOR_LIGHT_GRAY);
        }
    }
}

__attribute__((visibility("default"))) int CmdAppInit(const GUI_APP_API *api) {
    g_api = api;
    control_class = 0xFFFFFFFFU;
    control_window = 0xFFFFFFFFU;
    control_exit_requested = 0;
    g_item_count = 0;
    g_selected = 0;
    memset(g_items, 0, sizeof(g_items));
    return 1;
}

__attribute__((visibility("default"))) GUI_HANDLE CmdAppCreateMainWindow(void) {
    if (!g_api) return 0xFFFFFFFFU;
    if (control_class == 0xFFFFFFFFU) {
        control_class = g_api->RegisterClass("ControlPanelClass", 0, control_wndproc);
    }
    control_window = g_api->CreateWindowByClass(control_class, "Control Panel",
                                                96, 64, 480, 300, GUI_WS_OVERLAPPEDWINDOW);
    return control_window;
}

__attribute__((visibility("default"))) void CmdAppHandleKey(uint8_t scancode, char ascii, uint8_t pressed) {
    int cols;
    int width;
    (void)ascii;
    if (!pressed || g_item_count == 0 || !g_api) return;

    width = g_api->GetScreenWidth ? g_api->GetScreenWidth() : 640;
    cols = grid_columns(width - 64);

    switch (scancode) {
        case 0x01:
            control_exit_requested = 1;
            return;
        case 0x1C:
            launch_selected();
            return;
        case 0x4B:
            if (g_selected > 0) g_selected--;
            return;
        case 0x4D:
            if (g_selected + 1 < g_item_count) g_selected++;
            return;
        case 0x48:
            if (g_selected - cols >= 0) g_selected -= cols;
            return;
        case 0x50:
            if (g_selected + cols < g_item_count) g_selected += cols;
            return;
    }
}

__attribute__((visibility("default"))) void CmdAppHandleMouse(int x, int y, uint8_t buttons, uint8_t event_type) {
    int cols;
    int cell_w;
    int cell_h = 64;
    int grid_x = 8;
    int grid_y = 28;
    if (!g_api || event_type != GUI_MOUSE_LDOWN || !(buttons & 1)) return;

    cols = grid_columns(480 - 16);
    cell_w = (480 - 16 - ((cols - 1) * 8)) / cols;
    if (cell_w < 120) cell_w = 120;

    if (y >= grid_y) {
        int rel_y = y - grid_y;
        int row = rel_y / (cell_h + 8);
        int col = (x - grid_x) / (cell_w + 8);
        int item = row * cols + col;
        if (x >= grid_x && col >= 0 && col < cols &&
            (x - grid_x) % (cell_w + 8) < cell_w &&
            rel_y % (cell_h + 8) < cell_h &&
            item >= 0 && item < g_item_count) {
            if (g_selected == item) {
                launch_selected();
                return;
            }
            g_selected = item;
        }
    }
}

__attribute__((visibility("default"))) int CmdAppShouldExit(void) {
    return control_exit_requested;
}

__attribute__((visibility("default"))) void CmdAppResetExit(void) {
    control_exit_requested = 0;
}

int main(void) {
    return 0;
}
