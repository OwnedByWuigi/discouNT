#include <stdint.h>
#include "guiapp.h"

#define CMD_COLS 72
#define CMD_ROWS 22

#define COLOR_BLACK       0
#define COLOR_LIGHT_GRAY  7
#define COLOR_WHITE       15

static const GUI_APP_API *g_api = 0;
static GUI_HANDLE cmd_class = 0xFFFFFFFFU;
static GUI_HANDLE cmd_window = 0xFFFFFFFFU;
static int cmd_exit_requested = 0;

static char line_buf[CMD_ROWS][CMD_COLS + 1];
static int line_count = 0;
static char input_buf[CMD_COLS + 1];
static int input_len = 0;
static char current_path[256];
static char exec_path[256];
static uint32_t cmd_pid = 0;

extern void *memcpy(void *d, const void *s, uint32_t n);
extern uint32_t strlen(const char *s);
extern int strcmp(const char *a, const char *b);
extern void strcpy(char *d, const char *s);
extern void strcat(char *d, const char *s);
extern char *itoa(int value, char *str, int base);

static void uppercase_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i++] = c;
    }
    dst[i] = 0;
}

static void trim_spaces(char *s) {
    int start = 0;
    int end;
    int i;
    while (s && s[start] == ' ') start++;
    if (s && start > 0) {
        for (i = 0; s[start + i]; i++) s[i] = s[start + i];
        s[i] = 0;
    }
    if (!s) return;
    end = (int)strlen(s);
    while (end > 0 && s[end - 1] == ' ') s[--end] = 0;
}

static void path_join(char *out_path, const char *base, const char *name) {
    if (name[0] == '/') {
        strcpy(out_path, name);
        return;
    }
    strcpy(out_path, base);
    if (out_path[0] == 0) strcpy(out_path, "/");
    if (out_path[strlen(out_path) - 1] != '/') strcat(out_path, "/");
    strcat(out_path, name);
}

static int has_extension(const char *name) {
    int last_dot = -1;
    int last_slash = -1;
    int i;
    for (i = 0; name[i]; i++) {
        if (name[i] == '/') last_slash = i;
        else if (name[i] == '.') last_dot = i;
    }
    return (last_dot > last_slash);
}

static void ensure_exe_name(char *dst, const char *src, int max_len) {
    uppercase_copy(dst, src, max_len);
    if (!has_extension(dst) && ((int)strlen(dst) + 4) < max_len) strcat(dst, ".EXE");
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

static int resolve_directory_path(const char *input, uint32_t *out_lba, uint32_t *out_size, char *out_path) {
    uint8_t sector[2048];
    uint32_t dir_lba;
    uint32_t dir_size;
    char working[256];
    char canonical[256];
    char *part;
    uint32_t next_lba;
    uint32_t next_size;
    int is_dir;

    if (!g_api || !g_api->ReadSector || !input || !*input) return 0;
    if (!g_api->ReadSector(16, sector)) return 0;

    dir_lba = *(uint32_t*)(sector + 156 + 2);
    dir_size = *(uint32_t*)(sector + 156 + 10);

    if (input[0] == '/') {
        uppercase_copy(working, input + 1, sizeof(working));
        strcpy(canonical, "/");
    } else {
        uppercase_copy(working, input, sizeof(working));
        strcpy(canonical, current_path);
    }

    if (input[0] != '/' && current_path[1] != 0) {
        char replay[256];
        strcpy(replay, current_path + 1);
        part = replay;
        while (part && *part) {
            char *next = 0;
            for (int i = 0; part[i]; i++) {
                if (part[i] == '/') { part[i] = 0; next = part + i + 1; break; }
            }
            if (*part) {
                if (!sector_find_entry(dir_lba, dir_size, part, &next_lba, &next_size, &is_dir) || !is_dir) return 0;
                dir_lba = next_lba;
                dir_size = next_size;
            }
            part = next;
        }
    }

    if (working[0] == 0) {
        *out_lba = dir_lba;
        *out_size = dir_size;
        strcpy(out_path, canonical);
        return 1;
    }

    part = working;
    while (part && *part) {
        char *next = 0;
        for (int i = 0; part[i]; i++) {
            if (part[i] == '/') { part[i] = 0; next = part + i + 1; break; }
        }

        if (strcmp(part, ".") == 0 || part[0] == 0) {
        } else if (strcmp(part, "..") == 0) {
            if (canonical[1] != 0) {
                int last_slash = 0;
                for (int i = 0; canonical[i]; i++) if (canonical[i] == '/' && i > 0) last_slash = i;
                canonical[last_slash] = 0;
                if (canonical[0] == 0) { canonical[0] = '/'; canonical[1] = 0; }
            }
            dir_lba = *(uint32_t*)(sector + 156 + 2);
            dir_size = *(uint32_t*)(sector + 156 + 10);
            if (canonical[1] != 0) {
                char replay2[256];
                char *seg;
                strcpy(replay2, canonical + 1);
                seg = replay2;
                while (seg && *seg) {
                    char *seg_next = 0;
                    for (int i = 0; seg[i]; i++) {
                        if (seg[i] == '/') { seg[i] = 0; seg_next = seg + i + 1; break; }
                    }
                    if (*seg) {
                        if (!sector_find_entry(dir_lba, dir_size, seg, &next_lba, &next_size, &is_dir) || !is_dir) return 0;
                        dir_lba = next_lba;
                        dir_size = next_size;
                    }
                    seg = seg_next;
                }
            }
        } else {
            if (!sector_find_entry(dir_lba, dir_size, part, &next_lba, &next_size, &is_dir) || !is_dir) return 0;
            dir_lba = next_lba;
            dir_size = next_size;
            if (canonical[strlen(canonical) - 1] != '/') strcat(canonical, "/");
            strcat(canonical, part);
        }
        part = next;
    }

    *out_lba = dir_lba;
    *out_size = dir_size;
    strcpy(out_path, canonical);
    return 1;
}

static int resolve_exec_path(const char *name, char *resolved_path) {
    char normalized[256];
    char candidate[256];
    char paths[256];
    char dirpath[256];
    char *entry;
    uint32_t lba, size;
    int is_dir;
    int slash = -1;

    ensure_exe_name(normalized, name, sizeof(normalized));
    if (normalized[0] == '/') strcpy(candidate, normalized);
    else path_join(candidate, current_path, normalized);

    strcpy(dirpath, candidate);
    for (int i = 0; dirpath[i]; i++) if (dirpath[i] == '/') slash = i;
    if (slash >= 0) {
        dirpath[slash] = 0;
        if (dirpath[0] == 0) strcpy(dirpath, "/");
        if (resolve_directory_path(dirpath, &lba, &size, paths)) {
            if (sector_find_entry(lba, size, candidate + slash + 1, &lba, &size, &is_dir) && !is_dir) {
                strcpy(resolved_path, candidate);
                return 1;
            }
        }
    }

    uppercase_copy(paths, exec_path, sizeof(paths));
    entry = paths;
    while (entry && *entry) {
        char *next = 0;
        trim_spaces(entry);
        for (int i = 0; entry[i]; i++) {
            if (entry[i] == ';') { entry[i] = 0; next = entry + i + 1; break; }
        }
        trim_spaces(entry);
        if (*entry && resolve_directory_path(entry, &lba, &size, candidate)) {
            if (sector_find_entry(lba, size, normalized, &lba, &size, &is_dir) && !is_dir) {
                path_join(resolved_path, entry, normalized);
                return 1;
            }
        }
        entry = next;
    }
    return 0;
}

static void cmd_append_line(const char *text) {
    if (line_count == CMD_ROWS) {
        for (int i = 1; i < CMD_ROWS; i++) {
            strcpy(line_buf[i - 1], line_buf[i]);
        }
        line_count = CMD_ROWS - 1;
    }

    {
        int len = (int)strlen(text);
        if (len > CMD_COLS) len = CMD_COLS;
        memcpy(line_buf[line_count], text, (uint32_t)len);
        line_buf[line_count][len] = 0;
        line_count++;
    }
}

static void cmd_prompt_line(void) {
    char line[CMD_COLS + 1];
    strcpy(line, current_path);
    strcat(line, ">");
    strcat(line, input_buf);
    cmd_append_line(line);
}

static void cmd_clear_lines(void) {
    for (int i = 0; i < CMD_ROWS; i++) line_buf[i][0] = 0;
    line_count = 0;
}

static const char *skip_spaces(const char *s) {
    while (s && *s == ' ') s++;
    return s;
}

static void cmd_process_input(void) {
    char raw[CMD_COLS + 1];
    char cmd[CMD_COLS + 1];
    char *args = 0;
    int i;
    char pathbuf[256];
    char buf[32];

    cmd_prompt_line();

    for (i = 0; i < input_len; i++) {
        char c = input_buf[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        raw[i] = c;
    }
    raw[i] = 0;
    strcpy(cmd, raw);

    for (i = 0; cmd[i]; i++) {
        if (cmd[i] == ' ') {
            cmd[i] = 0;
            args = raw + i + 1;
            trim_spaces(args);
            break;
        }
    }

    if (strcmp(cmd, "HELP") == 0) {
        cmd_append_line("Commands: HELP CLS INFO DIR LS CD PWD");
        cmd_append_line("          PATH EXEC PING REBOOT SHUTDOWN EXIT");
    } else if (strcmp(cmd, "VER") == 0 || strcmp(cmd, "INFO") == 0) {
        cmd_append_line("discouNT Win32 Command Prompt");
        cmd_append_line("Filesystem: CDFS (ISO 9660)");
    } else if (strcmp(cmd, "CLS") == 0) {
        cmd_clear_lines();
    } else if (strcmp(cmd, "PWD") == 0) {
        cmd_append_line(current_path);
    } else if (strcmp(cmd, "PATH") == 0) {
        char line[CMD_COLS + 1];
        strcpy(line, "PATH=");
        strcat(line, exec_path);
        cmd_append_line(line);
    } else if (strcmp(cmd, "DIR") == 0 || strcmp(cmd, "LS") == 0) {
        uint32_t dir_lba, dir_size;
        uint8_t sector[2048];
        char line[CMD_COLS + 1];
        char num[16];
        if (!resolve_directory_path(current_path, &dir_lba, &dir_size, pathbuf)) {
            cmd_append_line("Directory unavailable");
        } else {
            uint32_t sectors = (dir_size + 2047) / 2048;
            int count = 2;
            strcpy(line, "Directory of ");
            strcat(line, current_path);
            cmd_append_line(line);
            cmd_append_line("<DIR>   .");
            cmd_append_line("<DIR>   ..");
            for (uint32_t s = 0; s < sectors; s++) {
                int off = 0;
                if (!g_api->ReadSector(dir_lba + s, sector)) break;
                while (off < 2048) {
                    uint8_t len = sector[off];
                    if (len == 0) break;
                    {
                        uint8_t flags = sector[off + 25];
                        uint8_t name_len = sector[off + 32];
                        char *name = (char*)(sector + off + 33);
                        uint32_t fsize = *(uint32_t*)(sector + off + 10);
                        int pos = 0;
                        if (name_len == 1 && (name[0] == 0 || name[0] == 1)) {
                            off += len;
                            continue;
                        }
                        if (flags & 0x02) strcpy(line, "<DIR>   ");
                        else strcpy(line, "        ");
                        pos = (int)strlen(line);
                        for (int n = 0; n < name_len && pos < CMD_COLS; n++) {
                            if (name[n] == ';') break;
                            line[pos++] = name[n];
                        }
                        line[pos] = 0;
                        if (!(flags & 0x02) && pos < CMD_COLS - 4) {
                            strcat(line, " (");
                            itoa((int)fsize, num, 10);
                            strcat(line, num);
                            strcat(line, ")");
                        }
                        cmd_append_line(line);
                        count++;
                    }
                    off += len;
                }
            }
            itoa(count, num, 10);
            strcpy(line, num);
            strcat(line, " entries");
            cmd_append_line(line);
        }
    } else if (strcmp(cmd, "CD") == 0) {
        uint32_t new_lba, new_size;
        if (!args || !*args) cmd_append_line(current_path);
        else if (resolve_directory_path(args, &new_lba, &new_size, pathbuf)) {
            strcpy(current_path, pathbuf);
            strcpy(line_buf[0], "");
            cmd_append_line("Changed directory");
        } else {
            cmd_append_line("Directory not found");
        }
    } else if (strcmp(cmd, "REBOOT") == 0) {
        cmd_append_line("Rebooting...");
        g_api->Reboot();
    } else if (strcmp(cmd, "SHUTDOWN") == 0) {
        cmd_append_line("Shutting down...");
        g_api->Shutdown();
    } else if (strcmp(cmd, "EXIT") == 0) {
        cmd_append_line("Closing session...");
        cmd_exit_requested = 1;
    } else if (strcmp(cmd, "PING") == 0) {
        char ping_out[CMD_COLS + 1];
        if (!args || !*skip_spaces(args)) {
            cmd_append_line("Usage: PING a.b.c.d");
        } else if (!g_api->Ping) {
            cmd_append_line("Ping unavailable");
        } else {
            ping_out[0] = 0;
            g_api->Ping(skip_spaces(args), ping_out, sizeof(ping_out));
            cmd_append_line(ping_out[0] ? ping_out : "Ping failed");
        }
    } else if (strcmp(cmd, "EXEC") == 0 && args && *args) {
        if (resolve_exec_path(args, pathbuf)) {
            int ret = g_api->ExecuteImage(pathbuf);
            strcpy(buf, "Exit code ");
            itoa(ret, buf + 10, 10);
            cmd_append_line(buf);
        } else {
            cmd_append_line("File not found in current directory or PATH");
        }
    } else if (cmd[0] != 0) {
        if (resolve_exec_path(raw, pathbuf)) {
            int ret = g_api->ExecuteImage(pathbuf);
            strcpy(buf, "Exit code ");
            itoa(ret, buf + 10, 10);
            cmd_append_line(buf);
        } else {
            cmd_append_line("Bad command or file name");
        }
    }

    input_len = 0;
    input_buf[0] = 0;
}

static void cmd_wndproc(GUI_HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == GUI_WM_CREATE) {
        cmd_clear_lines();
        cmd_append_line("discouNT [Version 0.0.3]");
        cmd_append_line("(c) wuggy 2026");
        cmd_append_line("");
        return;
    }

    if (msg == GUI_WM_PAINT && g_api) {
        GUI_RECT client;
        GUI_RECT win;
        int client_x;
        int client_y;

        g_api->GetClientRect(hwnd, &client);
        g_api->GetWindowRect(hwnd, &win);

        client_x = win.left + client.left;
        client_y = win.top + client.top;

        g_api->FillRect(client_x, client_y,
                        client.right - client.left, client.bottom - client.top, COLOR_BLACK);

        for (int i = 0; i < line_count; i++) {
            g_api->DrawString(client_x + 8, client_y + 8 + (i * 10),
                              line_buf[i], COLOR_LIGHT_GRAY, COLOR_BLACK);
        }

        {
            char prompt[CMD_COLS + 4];
            strcpy(prompt, "D:\\>");
            strcat(prompt, input_buf);
            g_api->DrawString(client_x + 8, client_y + 8 + (line_count * 10),
                              prompt, COLOR_WHITE, COLOR_BLACK);

            if ((int)strlen(prompt) < CMD_COLS) {
                int cursor_x = client_x + 8 + ((int)strlen(prompt) * 8);
                int cursor_y = client_y + 8 + (line_count * 10);
                g_api->FillRect(cursor_x, cursor_y + 8, 7, 1, COLOR_WHITE);
            }
        }
    }
}

__attribute__((visibility("default"))) int CmdAppInit(const GUI_APP_API *api) {
    g_api = api;
    cmd_class = 0xFFFFFFFFU;
    cmd_window = 0xFFFFFFFFU;
    cmd_exit_requested = 0;
    input_len = 0;
    input_buf[0] = 0;
    strcpy(current_path, "/");
    strcpy(exec_path, "/SYSTEM32");
    cmd_pid = (g_api && g_api->GetProcessId) ? g_api->GetProcessId() : 0;
    cmd_clear_lines();
    return 1;
}

__attribute__((visibility("default"))) GUI_HANDLE CmdAppCreateMainWindow(void) {
    if (!g_api) return 0xFFFFFFFFU;

    if (cmd_class == 0xFFFFFFFFU) {
        cmd_class = g_api->RegisterClass("GuiCmdClass", 0, cmd_wndproc);
        if (cmd_class == 0xFFFFFFFFU) {
            return 0xFFFFFFFFU;
        }
    }

    cmd_exit_requested = 0;
    input_len = 0;
    input_buf[0] = 0;

    if (g_api->CreateWindowByClass) {
        cmd_window = g_api->CreateWindowByClass(cmd_class, "Command Prompt",
                                                72, 52, 640, 320, GUI_WS_OVERLAPPEDWINDOW);
    }
    if (cmd_window == 0xFFFFFFFFU && g_api->CreateWindow) {
        cmd_window = g_api->CreateWindow("GuiCmdClass", "Command Prompt",
                                         72, 52, 640, 320, GUI_WS_OVERLAPPEDWINDOW);
    }
    return cmd_window;
}

__attribute__((visibility("default"))) void CmdAppHandleKey(uint8_t scancode, char ascii, uint8_t pressed) {
    if (!pressed) return;

    switch (scancode) {
        case 0x0E:
            if (input_len > 0) {
                input_len--;
                input_buf[input_len] = 0;
            }
            return;
        case 0x1C:
            cmd_process_input();
            return;
    }

    if (input_len >= CMD_COLS) return;
    if (ascii) {
        input_buf[input_len++] = ascii;
        input_buf[input_len] = 0;
    }
}

__attribute__((visibility("default"))) void CmdAppHandleMouse(int x, int y, uint8_t buttons, uint8_t event_type) {
    (void)x;
    (void)y;
    (void)buttons;
    (void)event_type;
}

__attribute__((visibility("default"))) int CmdAppShouldExit(void) {
    return cmd_exit_requested;
}

__attribute__((visibility("default"))) void CmdAppResetExit(void) {
    cmd_exit_requested = 0;
}

int main(void) {
    return 0;
}
