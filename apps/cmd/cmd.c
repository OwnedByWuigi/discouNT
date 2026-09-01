#include <stdint.h>
#include "guiapp.h"
#include "core/version.h"

#define CMD_COLS 72
#define CMD_ROWS 128
#define CMD_SCROLL_SIZE 14

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
/* Number of output rows scrolled back from the live prompt. */
static int cmd_scroll = 0;
static int cmd_scroll_drag = 0;
static int cmd_scroll_drag_origin = 0;
static int cmd_scroll_drag_start = 0;
static char current_path[256];
static char exec_path[256];
static uint32_t cmd_pid = 0;

extern void *memcpy(void *d, const void *s, uint32_t n);
extern uint32_t strlen(const char *s);
extern int strcmp(const char *a, const char *b);
extern void strcpy(char *d, const char *s);
extern void strcat(char *d, const char *s);
extern char *itoa(int value, char *str, int base);
#if defined(__loongarch64)
extern void SerialPutString(const char *str);
#define CMD_LA_TRACE(text) SerialPutString(text)
#else
#define CMD_LA_TRACE(text) ((void)0)
#endif

static int cmd_visible_rows(int client_h) {
    int rows = (client_h - 16) / 10;
    return rows > 1 ? rows : 1;
}

static int cmd_scroll_max(int client_h) {
    int total = line_count + 1;
    int max = total - cmd_visible_rows(client_h);
    return max > 0 ? max : 0;
}

static void cmd_clamp_scroll(int client_h) {
    int max = cmd_scroll_max(client_h);
    if (cmd_scroll < 0) cmd_scroll = 0;
    if (cmd_scroll > max) cmd_scroll = max;
}

static void cmd_redraw(void) {
    if (g_api && g_api->UpdateWindow && cmd_window != 0xFFFFFFFFU) g_api->UpdateWindow(cmd_window);
}

/* Repaint only the console scrollbar column. Dragging the thumb must not
 * tear the whole console down to black and re-render every glyph on each
 * pointer move: the text does not move relative to the window during a
 * drag, only the thumb does, so a full repaint is pure flicker. */
static void cmd_draw_scrollbar(int client_x, int client_y, int client_w, int client_h,
                               int visible_rows, int total_rows) {
    int sx;
    int sh;
    int track;
    int max;
    int thumb;
    int travel;
    int thumb_y;

    if (!g_api) return;
    if (client_w < CMD_SCROLL_SIZE + 24) return;

    sx = client_x + client_w - CMD_SCROLL_SIZE;
    sh = client_h;
    track = sh - CMD_SCROLL_SIZE * 2;
    if (track <= 0) return;
    max = cmd_scroll_max(client_h);
    thumb = max > 0 ? (visible_rows * track) / total_rows : track;
    if (thumb < 8) thumb = 8;
    if (thumb > track) thumb = track;
    travel = track - thumb;
    thumb_y = CMD_SCROLL_SIZE + (max > 0 ? (max - cmd_scroll) * travel / max : 0);
    g_api->FillRect(sx, client_y, CMD_SCROLL_SIZE, sh, COLOR_LIGHT_GRAY);
    g_api->DrawRect(sx, client_y, CMD_SCROLL_SIZE, sh, COLOR_BLACK);
    g_api->DrawString(sx + 3, client_y + 2, "^", COLOR_BLACK, COLOR_LIGHT_GRAY);
    g_api->DrawString(sx + 3, client_y + sh - 11, "v", COLOR_BLACK, COLOR_LIGHT_GRAY);
    g_api->FillRect(sx + 2, client_y + thumb_y, CMD_SCROLL_SIZE - 4, thumb, COLOR_WHITE);
    g_api->DrawRect(sx + 2, client_y + thumb_y, CMD_SCROLL_SIZE - 4, thumb, COLOR_BLACK);
}

/* Derive the client screen origin (border offset) and redraw only the
 * scrollbar column of the console window. */
static void cmd_redraw_scrollbar(void) {
    GUI_RECT client;
    GUI_RECT win;
    int win_w;
    int win_h;
    int client_w;
    int client_h;
    int border_x;
    int border_y;
    int client_x;
    int client_y;

    if (!g_api || !g_api->GetClientRect || !g_api->GetWindowRect) return;
    if (cmd_window == 0xFFFFFFFFU) return;
    g_api->GetClientRect(cmd_window, &client);
    g_api->GetWindowRect(cmd_window, &win);

    win_w = win.right - win.left;
    win_h = win.bottom - win.top;
    client_w = client.right - client.left;
    client_h = client.bottom - client.top;

    border_x = (win_w > client_w) ? ((win_w - client_w) / 2) : 0;
    border_y = (win_h > client_h) ? (win_h - client_h - border_x) : 0;

    client_x = win.left + border_x;
    client_y = win.top + border_y;

    cmd_draw_scrollbar(client_x, client_y, client_w, client_h,
                       cmd_visible_rows(client_h), line_count + 1);
}

static void uppercase_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i++] = c;
    }
    dst[i] = 0;
}

static int cmd_path_name_equal(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
        if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
        if (ca != cb) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
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
                if (cmd_path_name_equal(name, entry)) {
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

/* Resolve only argv[0].  Previously CMD handed the whole input line to the
   filesystem resolver, so "program.exe argument" was searched as one long
   filename and arguments could never reach CSRSS. */
static int cmd_launch_external(const char *command_line) {
    char image[256];
    char resolved[256];
    char launch[512];
    const char *p = command_line;
    const char *arguments;
    char quote = 0;
    int n = 0;

    if (!g_api || !g_api->ExecuteImage || !p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\"' || *p == '\'') quote = *p++;
    while (*p && n < (int)sizeof(image) - 1) {
        if ((quote && *p == quote) || (!quote && (*p == ' ' || *p == '\t'))) break;
        image[n++] = *p++;
    }
    image[n] = 0;
    if (quote && *p == quote) p++;
    while (*p == ' ' || *p == '\t') p++;
    arguments = p;

    if (!image[0] || !resolve_exec_path(image, resolved)) return 0;
    strcpy(launch, resolved);
    if (*arguments && strlen(launch) + strlen(arguments) + 2 < sizeof(launch)) {
        strcat(launch, " ");
        strcat(launch, arguments);
    }
    g_api->ExecuteImage(launch);
    return 1;
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
    /* New command output follows the live prompt. */
    cmd_scroll = 0;
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
    cmd_scroll = 0;
}

/* Kernel32 routes stdout/stderr from standard console programs here while
 * this GUI command prompt owns the console. */
__attribute__((visibility("default"))) void CmdAppWriteConsole(const char *text, uint32_t length) {
    char line[CMD_COLS + 1];
    uint32_t i;
    int n = 0;
    if (!text) return;
    for (i = 0; i < length; i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\n') {
            line[n] = 0;
            cmd_append_line(line);
            n = 0;
        } else if (n < CMD_COLS) {
            line[n++] = c;
        }
    }
    if (n) {
        line[n] = 0;
        cmd_append_line(line);
    }
    cmd_redraw();
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
    const char *original_args = 0;

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
            original_args = input_buf + i + 1;
            trim_spaces(args);
            original_args = skip_spaces(original_args);
            break;
        }
    }

    if (strcmp(cmd, "HELP") == 0) {
        cmd_append_line("Commands: HELP CLS INFO DIR LS CD PWD");
        cmd_append_line("          PATH EXEC PING REBOOT SHUTDOWN EXIT");
    } else if (strcmp(cmd, "INDUCECRASHFORREALZ") == 0) {
        if (g_api && g_api->BugCheck) {
            g_api->BugCheck(0xDEAD0001U, 0x434D4443U, cmd_pid, 0, 0);
        }
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
            cmd_append_line("Usage: PING host-or-address");
        } else if (!g_api->Ping) {
            cmd_append_line("Ping unavailable");
        } else {
            ping_out[0] = 0;
            g_api->Ping(skip_spaces(args), ping_out, sizeof(ping_out));
            cmd_append_line(ping_out[0] ? ping_out : "Ping failed");
        }
    } else if (strcmp(cmd, "EXEC") == 0 && args && *args) {
        if (!cmd_launch_external(original_args)) {
            cmd_append_line("File not found in current directory or PATH");
        }
    } else if (cmd[0] != 0) {
        if (!cmd_launch_external(input_buf)) {
            cmd_append_line("Bad command or file name");
        }
    }

    input_len = 0;
    input_buf[0] = 0;
}

static void cmd_handle_scroll_mouse(int x, int y, uint8_t event_type) {
    GUI_RECT rc;
    int track, visible, max, total, thumb, travel, thumb_y;
    int sx;
    if (!g_api || !g_api->GetClientRect) return;
    g_api->GetClientRect(cmd_window, &rc);
    if (event_type == GUI_MOUSE_WHEEL) {
        int delta = (int)(int8_t)x;
        cmd_scroll += delta > 0 ? delta : delta;
        cmd_clamp_scroll(rc.bottom);
        cmd_redraw();
        return;
    }
    sx = rc.right - CMD_SCROLL_SIZE;
    if (sx < 0 || y < 0 || y >= rc.bottom) return;
    /* Once the thumb is grabbed, continue tracking even if the pointer
     * leaves the narrow scrollbar column. */
    if (!cmd_scroll_drag && x < sx) return;
    visible = cmd_visible_rows(rc.bottom);
    max = cmd_scroll_max(rc.bottom);
    total = line_count + 1;
    track = rc.bottom - CMD_SCROLL_SIZE * 2;
    if (track <= 0) return;
    thumb = max > 0 ? (visible * track) / total : track;
    if (thumb < 8) thumb = 8;
    if (thumb > track) thumb = track;
    travel = track - thumb;
    thumb_y = CMD_SCROLL_SIZE + (max > 0 ? (max - cmd_scroll) * travel / max : 0);

    if (event_type == GUI_MOUSE_LDOWN) {
        if (max <= 0) return;
        if (y < CMD_SCROLL_SIZE) cmd_scroll += 1;
        else if (y >= rc.bottom - CMD_SCROLL_SIZE) cmd_scroll -= 1;
        else if (y < thumb_y) cmd_scroll += visible;
        else if (y >= thumb_y + thumb) cmd_scroll -= visible;
        else {
            cmd_scroll_drag = 1;
            cmd_scroll_drag_origin = y;
            cmd_scroll_drag_start = cmd_scroll;
            return;
        }
        cmd_clamp_scroll(rc.bottom);
        cmd_redraw();
    } else if (event_type == GUI_MOUSE_MOVE && cmd_scroll_drag) {
        int prev_scroll = cmd_scroll;
        if (travel > 0) cmd_scroll = cmd_scroll_drag_start -
            ((y - cmd_scroll_drag_origin) * max) / travel;
        cmd_clamp_scroll(rc.bottom);
        /* Only the thumb moves while dragging; never rebuild the whole
         * console per pointer event or the window flickers. */
        if (cmd_scroll != prev_scroll) cmd_redraw_scrollbar();
    } else if (event_type == GUI_MOUSE_LUP && cmd_scroll_drag) {
        cmd_scroll_drag = 0;
        /* The text is unchanged since the last drag-move repaint. */
        cmd_redraw_scrollbar();
    }
}

static void cmd_wndproc(GUI_HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)wParam;
    (void)lParam;

    if (msg == GUI_WM_CREATE) {
        cmd_clear_lines();
        cmd_append_line(DISCOUNT_NAME " [Version " DISCOUNT_VERSION "]");
        cmd_append_line("(c) wuggy 2026");
        cmd_append_line("");
        return;
    }

    if (msg == GUI_WM_PAINT && g_api) {
        GUI_RECT client;
        GUI_RECT win;
        int client_x;
        int client_y;
        int win_w;
        int win_h;
        int client_w;
        int client_h;
        int border_x;
        int border_y;
        int visible_rows;
        int total_rows;
        int first_line;

        g_api->GetClientRect(hwnd, &client);
        g_api->GetWindowRect(hwnd, &win);

        win_w = win.right - win.left;
        win_h = win.bottom - win.top;
        client_w = client.right - client.left;
        client_h = client.bottom - client.top;

        border_x = (win_w > client_w) ? ((win_w - client_w) / 2) : 0;
        border_y = (win_h > client_h) ? (win_h - client_h - border_x) : 0;

        client_x = win.left + border_x;
        client_y = win.top + border_y;

        visible_rows = cmd_visible_rows(client_h);
        cmd_clamp_scroll(client_h);
        total_rows = line_count + 1;
        first_line = total_rows - visible_rows - cmd_scroll;
        if (first_line < 0) first_line = 0;

        g_api->FillRect(client_x, client_y, client_w, client_h, COLOR_BLACK);

        for (int i = 0; i < visible_rows && first_line + i < total_rows; i++) {
            int line_index = first_line + i;
            if (line_index < line_count) {
                g_api->DrawString(client_x + 8, client_y + 8 + (i * 10),
                                  line_buf[line_index], COLOR_LIGHT_GRAY, COLOR_BLACK);
            }
        }

        if (cmd_scroll == 0 && first_line + visible_rows - 1 >= line_count) {
            char prompt[CMD_COLS + 4];
            strcpy(prompt, "D:\\>");
            strcat(prompt, input_buf);
            g_api->DrawString(client_x + 8, client_y + 8 + ((line_count - first_line) * 10),
                              prompt, COLOR_WHITE, COLOR_BLACK);

            if ((int)strlen(prompt) < CMD_COLS) {
                int cursor_x = client_x + 8 + ((int)strlen(prompt) * 8);
                int cursor_y = client_y + 8 + ((line_count - first_line) * 10);
                g_api->FillRect(cursor_x, cursor_y + 8, 7, 1, COLOR_WHITE);
            }
        }

        /* Console scroll bar: the live prompt is the bottom of the range. */
        cmd_draw_scrollbar(client_x, client_y, client_w, client_h, visible_rows, total_rows);
    }
}

__attribute__((visibility("default"))) int CmdAppInit(const GUI_APP_API *api) {
    CMD_LA_TRACE("[CMD] Init entered\r\n");
    g_api = api;
    cmd_class = 0xFFFFFFFFU;
    cmd_window = 0xFFFFFFFFU;
    cmd_exit_requested = 0;
    input_len = 0;
    input_buf[0] = 0;
    cmd_scroll = 0;
    cmd_scroll_drag = 0;
    strcpy(current_path, "/");
    CMD_LA_TRACE("[CMD] Current path initialized\r\n");
    strcpy(exec_path, "/SYSTEM32");
    CMD_LA_TRACE("[CMD] Executable path initialized\r\n");
    cmd_pid = (g_api && g_api->GetProcessId) ? g_api->GetProcessId() : 0;
    CMD_LA_TRACE("[CMD] Process id initialized\r\n");
    cmd_clear_lines();
    CMD_LA_TRACE("[CMD] Init complete\r\n");
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
    cmd_scroll = 0;
    cmd_scroll_drag = 0;

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

    /* Typing always returns to the live prompt, like a real console. */
    cmd_scroll = 0;

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
    if (event_type == GUI_MOUSE_WHEEL) {
        cmd_handle_scroll_mouse((int)(int8_t)buttons, y, event_type);
        return;
    }
    cmd_handle_scroll_mouse(x, y, event_type);
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
