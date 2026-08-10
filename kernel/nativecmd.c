#include <stdint.h>
#include "nativecmd.h"
#include "hal.h"
#include "mm.h"
#include "portio.h"
#include "util.h"
#include "serial.h"
#include "cdfs.h"
#include "peloader.h"
#include "subsystem.h"
#include "version.h"

static char cmd_buffer[256];
static int cmd_pos = 0;

static uint32_t current_dir_lba = 0;
static uint32_t current_dir_size = 0;
static char current_path[256] = "/";
static char exec_path[256] = "/SYSTEM32";

#define SMSS_RETURN_MAGIC 0x534D5353

static void system_shutdown(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

static void show_prompt(void) {
    HalPutString("\n", 0x0F);
    HalPutString(current_path, 0x1F);
    HalPutString("> ", 0x1F);
}

static void init_current_dir(void) {
    uint8_t sector[2048];
    if (CdfsReadSector(16, sector)) {
        uint8_t *root = sector + 156;
        current_dir_lba = *(uint32_t*)(root + 2);
        current_dir_size = *(uint32_t*)(root + 10);
    }
    current_path[0] = '/';
    current_path[1] = 0;
}

static void uppercase_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i] = c;
        i++;
    }
    dst[i] = 0;
}

static void trim_spaces(char *s) {
    int start = 0;
    int end;
    int i;

    if (!s) return;

    while (s[start] == ' ') start++;
    if (start > 0) {
        for (i = 0; s[start + i]; i++) s[i] = s[start + i];
        s[i] = 0;
    }

    end = (int)strlen(s);
    while (end > 0 && s[end - 1] == ' ') {
        s[end - 1] = 0;
        end--;
    }
}

static int find_in_dir(uint32_t dir_lba, uint32_t dir_size, const char *name,
                       uint32_t *out_lba, uint32_t *out_size, int *is_dir) {
    uint32_t sectors = (dir_size + 2047) / 2048;
    uint8_t *dir = (uint8_t*)kmalloc(sectors * 2048);
    if (!dir) return 0;

    for (uint32_t i = 0; i < sectors; i++) {
        CdfsReadSector(dir_lba + i, dir + i * 2048);
    }

    {
        int off = 0;
        int found = 0;

        while (off < (int)(sectors * 2048)) {
            uint8_t len = dir[off];
            if (len == 0) break;

            uint8_t flags = dir[off + 25];
            uint8_t name_len = dir[off + 32];
            char *dname = (char*)(dir + off + 33);

            char entry[256];
            int ei = 0;
            for (int i = 0; i < name_len && ei < 254; i++) {
                if (dname[i] == ';') break;
                entry[ei++] = dname[i];
            }
            entry[ei] = 0;

            if (strcmp(name, entry) == 0) {
                *out_lba = *(uint32_t*)(dir + off + 2);
                *out_size = *(uint32_t*)(dir + off + 10);
                *is_dir = (flags & 0x02) ? 1 : 0;
                found = 1;
                break;
            }
            off += len;
        }

        kfree(dir);
        return found;
    }
}

static void path_join(char *out_path, const char *base, const char *name) {
    if (!out_path || !base || !name) return;

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
    if (!has_extension(dst) && ((int)strlen(dst) + 4) < max_len) {
        strcat(dst, ".EXE");
    }
}

static int try_load_exec_path(const char *path, uint8_t **out_buf, uint32_t *out_size, char *resolved_path) {
    if (CdfsReadFile(path, out_buf, out_size)) {
        if (resolved_path) strcpy(resolved_path, path);
        return 1;
    }
    return 0;
}

static int load_exec_buffer(const char *name, uint8_t **out_buf, uint32_t *out_size, char *resolved_path) {
    char normalized_name[256];
    char candidate[256];
    char path_list[256];
    char *entry;

    ensure_exe_name(normalized_name, name, sizeof(normalized_name));

    if (normalized_name[0] == '/') {
        return try_load_exec_path(normalized_name, out_buf, out_size, resolved_path);
    }

    path_join(candidate, current_path, normalized_name);
    if (try_load_exec_path(candidate, out_buf, out_size, resolved_path)) {
        return 1;
    }

    uppercase_copy(path_list, exec_path, sizeof(path_list));
    entry = path_list;

    while (entry && *entry) {
        char *next = 0;
        trim_spaces(entry);

        for (int i = 0; entry[i]; i++) {
            if (entry[i] == ';') {
                entry[i] = 0;
                next = entry + i + 1;
                break;
            }
        }

        trim_spaces(entry);
        if (*entry) {
            path_join(candidate, entry, normalized_name);
            if (try_load_exec_path(candidate, out_buf, out_size, resolved_path)) {
                return 1;
            }
        }

        entry = next;
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

    if (!input || !*input || !out_lba || !out_size || !out_path) return 0;
    if (!CdfsReadSector(16, sector)) return 0;

    dir_lba = *(uint32_t*)(sector + 156 + 2);
    dir_size = *(uint32_t*)(sector + 156 + 10);

    if (input[0] == '/') {
        uppercase_copy(working, input + 1, sizeof(working));
        strcpy(canonical, "/");
    } else {
        uppercase_copy(working, input, sizeof(working));
        strcpy(canonical, current_path);
    }

    if (working[0] == 0) {
        *out_lba = dir_lba;
        *out_size = dir_size;
        strcpy(out_path, canonical);
        return 1;
    }

    if (input[0] != '/' && current_path[1] != 0) {
        char current_copy[256];
        strcpy(current_copy, current_path + 1);
        part = current_copy;
        while (part && *part) {
            char *next = 0;
            for (int i = 0; part[i]; i++) {
                if (part[i] == '/') {
                    part[i] = 0;
                    next = part + i + 1;
                    break;
                }
            }
            if (*part) {
                if (!find_in_dir(dir_lba, dir_size, part, &next_lba, &next_size, &is_dir) || !is_dir) {
                    return 0;
                }
                dir_lba = next_lba;
                dir_size = next_size;
            }
            part = next;
        }
    }

    part = working;
    while (part && *part) {
        char *next = 0;
        for (int i = 0; part[i]; i++) {
            if (part[i] == '/') {
                part[i] = 0;
                next = part + i + 1;
                break;
            }
        }

        if (strcmp(part, ".") == 0 || part[0] == 0) {
        } else if (strcmp(part, "..") == 0) {
            if (canonical[1] != 0) {
                int last_slash = 0;
                for (int i = 0; canonical[i]; i++) {
                    if (canonical[i] == '/' && i > 0) last_slash = i;
                }
                canonical[last_slash] = 0;
                if (canonical[0] == 0) {
                    canonical[0] = '/';
                    canonical[1] = 0;
                }
            }

            dir_lba = *(uint32_t*)(sector + 156 + 2);
            dir_size = *(uint32_t*)(sector + 156 + 10);
            if (canonical[1] != 0) {
                char replay[256];
                char *seg;
                strcpy(replay, canonical + 1);
                seg = replay;
                while (seg && *seg) {
                    char *seg_next = 0;
                    for (int i = 0; seg[i]; i++) {
                        if (seg[i] == '/') {
                            seg[i] = 0;
                            seg_next = seg + i + 1;
                            break;
                        }
                    }
                    if (*seg) {
                        if (!find_in_dir(dir_lba, dir_size, seg, &next_lba, &next_size, &is_dir) || !is_dir) {
                            return 0;
                        }
                        dir_lba = next_lba;
                        dir_size = next_size;
                    }
                    seg = seg_next;
                }
            }
        } else {
            if (!find_in_dir(dir_lba, dir_size, part, &next_lba, &next_size, &is_dir) || !is_dir) {
                return 0;
            }
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

static int is_pe_image(const uint8_t *file_buf, uint32_t file_size) {
    uint32_t pe_off;
    if (file_size < 64 || file_buf[0] != 0x4D || file_buf[1] != 0x5A) return 0;
    pe_off = *(uint32_t*)(file_buf + 0x3C);
    if (pe_off + 4 > file_size) return 0;
    return (file_buf[pe_off] == 'P' && file_buf[pe_off + 1] == 'E');
}

static int is_elf_image(const uint8_t *file_buf, uint32_t file_size) {
    if (file_size < 4) return 0;
    return (*(const uint32_t*)file_buf == 0x464C457F);
}

static void cmd_help(void) {
    HalPutString("\nCommands:\n", 0x0F);
    HalPutString("  help      - Show help\n", 0x0F);
    HalPutString("  clear     - Clear screen\n", 0x0F);
    HalPutString("  info      - System info\n", 0x0F);
    HalPutString("  ls/dir    - List files\n", 0x0F);
    HalPutString("  cd <dir>  - Change directory\n", 0x0F);
    HalPutString("  pwd       - Print working directory\n", 0x0F);
    HalPutString("  exec <f>  - Execute native/Win32 image\n", 0x0F);
    HalPutString("  path      - Show executable search path\n", 0x0F);
    HalPutString("  reboot    - Reboot system\n", 0x0F);
    HalPutString("  shutdown  - Power off system\n", 0x0F);
    HalPutString("\nWin32 session:\n", 0x0E);
    HalPutString("  smss\n", 0x0F);
}

static void cmd_clear(void) {
    HalClearScreen(0x1F);
}

static void cmd_info(void) {
    HalPutString("\n" DISCOUNT_NAME "\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Kernel: NT-like\n", 0x0F);
    HalPutString("Filesystem: CDFS (ISO 9660)\n", 0x0F);
    HalPutString("Shell: Native command processor\n", 0x0F);
}

static void cmd_pwd(void) {
    HalPutString("\n", 0x0F);
    HalPutString(current_path, 0x0F);
    HalPutString("\n", 0x0F);
}

static void cmd_path(void) {
    HalPutString("\nPATH=", 0x0F);
    HalPutString(exec_path, 0x0F);
    HalPutString("\n", 0x0F);
}

static void cmd_ls(void) {
    if (current_dir_size == 0) {
        HalPutString("\nCD not ready\n", 0x0C);
        return;
    }

    HalPutString("\n Directory of ", 0x0F);
    HalPutString(current_path, 0x0F);
    HalPutString("\n\n", 0x0F);

    {
        uint32_t sectors = (current_dir_size + 2047) / 2048;
        uint8_t *dir = (uint8_t*)kmalloc(sectors * 2048);
        char buf[16];
        int off = 0;
        int count = 2;

        if (!dir) {
            HalPutString("Out of memory\n", 0x0C);
            return;
        }

        for (uint32_t i = 0; i < sectors; i++) {
            CdfsReadSector(current_dir_lba + i, dir + i * 2048);
        }

        HalPutString("  <DIR>   .\n", 0x0E);
        HalPutString("  <DIR>   ..\n", 0x0E);

        while (off < (int)(sectors * 2048)) {
            uint8_t len = dir[off];
            if (len == 0) break;

            uint8_t flags = dir[off + 25];
            uint8_t name_len = dir[off + 32];
            char *name = (char*)(dir + off + 33);
            uint32_t fsize = *(uint32_t*)(dir + off + 10);

            HalPutString((flags & 0x02) ? "  <DIR>   " : "         ", (flags & 0x02) ? 0x0E : 0x0F);
            for (int i = 0; i < name_len; i++) {
                if (name[i] == ';') break;
                HalPutChar(name[i], (flags & 0x02) ? 0x0E : 0x0F);
            }
            if (!(flags & 0x02)) {
                HalPutString("  (", 0x0F);
                itoa(fsize, buf, 10);
                HalPutString(buf, 0x0F);
                HalPutString(" bytes)", 0x0F);
            }
            HalPutString("\n", 0x0F);

            count++;
            off += len;
        }

        HalPutString("\n", 0x0F);
        itoa(count, buf, 10);
        HalPutString(buf, 0x0F);
        HalPutString(" entries\n", 0x0F);
        kfree(dir);
    }
}

static void cmd_cd(char *args) {
    char dirname[256];
    uint32_t new_lba, new_size;
    char new_path[256];

    if (!args || !*args) {
        cmd_pwd();
        return;
    }

    uppercase_copy(dirname, args, sizeof(dirname));
    trim_spaces(dirname);

    if (resolve_directory_path(dirname, &new_lba, &new_size, new_path)) {
        current_dir_lba = new_lba;
        current_dir_size = new_size;
        strcpy(current_path, new_path);
        HalPutString("\nChanged to ", 0x0A);
        HalPutString(current_path, 0x0F);
        HalPutString("\n", 0x0A);
    } else {
        HalPutString("\nDirectory not found: ", 0x0C);
        HalPutString(dirname, 0x0C);
        HalPutString("\n", 0x0C);
    }
}

static int run_loaded_image(void *image, const char *display_name) {
    uint8_t *exe_stack;
    uint32_t exe_esp;
    uint32_t saved_esp;
    char buf[16];
    int ret = 0;
    typedef int (*EntryFunc)(void);
    EntryFunc func;

    if (!image) return -1;

    func = (EntryFunc)PeGetEntryPoint(image);
    if (!func) {
        HalPutString("No entry point\n", 0x0C);
        return -1;
    }

    SerialPutString("[EXEC] Entry: 0x");
    SerialPrintHex((uint32_t)func);
    SerialPutString("\r\n");

    HalPutString("Executing ", 0x0A);
    HalPutString(display_name, 0x0A);
    HalPutString("...\n", 0x0A);
    HalPutString("================================\n", 0x0F);

    exe_stack = (uint8_t*)kmalloc(65536);
    if (!exe_stack) {
        HalPutString("Out of memory\n", 0x0C);
        return -1;
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

    HalPutString("\n================================", 0x0F);
    HalPutString("\nProgram exited with code: ", 0x0A);
    itoa(ret, buf, 10);
    HalPutString(buf, 0x0A);
    HalPutString("\n", 0x0A);

    return ret;
}

static void cmd_exec(char *args) {
    char filename[256];
    char resolved_path[256];
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    void *image;
    int ret;
    int is_smss;

    if (!args || !*args) {
        HalPutString("\nUsage: exec <filename>\n", 0x0C);
        return;
    }

    uppercase_copy(filename, args, sizeof(filename));
    trim_spaces(filename);

    HalPutString("\nLoading: ", 0x0F);
    HalPutString(filename, 0x0F);
    HalPutString("...\n", 0x0F);

    if (!load_exec_buffer(filename, &file_buf, &file_size, resolved_path)) {
        HalPutString("File not found in current directory or PATH\n", 0x0C);
        return;
    }

    HalPutString("Resolved path: ", 0x0F);
    HalPutString(resolved_path, 0x0F);
    HalPutString("\n", 0x0F);

    if (!is_pe_image(file_buf, file_size) && !is_elf_image(file_buf, file_size)) {
        HalPutString("Not a supported executable image\n", 0x0C);
        kfree(file_buf);
        return;
    }

    image = PeLoadImage(file_buf, file_size);
    if (!image) {
        HalPutString("Failed to map image. Check serial.\n", 0x0C);
        kfree(file_buf);
        return;
    }

    if (!PeResolveImports(image)) {
        const char *error_text = PeGetLastError();
        HalPutString("Application Error: ", 0x0C);
        HalPutString(error_text ? error_text : "Missing imports.", 0x0C);
        HalPutString("\n", 0x0C);
        PeFreeImage(image);
        kfree(file_buf);
        return;
    }
    if (is_pe_image(file_buf, file_size)) {
        PePerformRelocations(image);
    }

    {
        char compare_name[256];
        ensure_exe_name(compare_name, filename, sizeof(compare_name));
        is_smss = (strcmp(compare_name, "SMSS.EXE") == 0);
    }
    ret = run_loaded_image(image, filename);

    if (is_smss && (ret == SMSS_RETURN_MAGIC || ret == 0)) {
        HalPutString("\nStarting Win32 subsystem via SMSS.EXE...\n", 0x0E);
        PeFreeImage(image);
        kfree(file_buf);
        SubsystemLaunchSmss();
        return;
    }

    PeFreeImage(image);
    kfree(file_buf);
}

static void cmd_reboot(void) {
    HalPutString("\nRebooting...\n", 0x0C);
    {
        uint8_t status;
        do { status = inb(0x64); } while (status & 0x02);
        outb(0x64, 0xFE);
        __asm__ volatile("int $0");
    }
}

static void cmd_shutdown(void) {
    HalPutString("\nShutting down...\n", 0x0C);
    system_shutdown();
}

static void process_command(void) {
    char *cmd;
    char *args = 0;

    cmd_buffer[cmd_pos] = 0;
    HalPutString("\n", 0x0F);
    SerialPutString("[CMD] ");
    SerialPutString(cmd_buffer);
    SerialPutString("\r\n");

    cmd = cmd_buffer;
    while (*cmd == ' ') cmd++;

    for (int i = 0; cmd_buffer[i]; i++) {
        if (cmd_buffer[i] == ' ') {
            cmd_buffer[i] = 0;
            args = &cmd_buffer[i + 1];
            while (*args == ' ') args++;
            break;
        }
    }

    if (strcmp(cmd, "help") == 0) cmd_help();
    else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) cmd_clear();
    else if (strcmp(cmd, "info") == 0) cmd_info();
    else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) cmd_ls();
    else if (strcmp(cmd, "cd") == 0) cmd_cd(args);
    else if (strcmp(cmd, "pwd") == 0) cmd_pwd();
    else if (strcmp(cmd, "path") == 0) cmd_path();
    else if (strcmp(cmd, "exec") == 0) cmd_exec(args);
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();
    else if (*cmd) {
        cmd_exec(cmd);
    }

    cmd_pos = 0;
    cmd_buffer[0] = 0;
    show_prompt();
}

void NativeCmdInit(void) {
    cmd_pos = 0;
    cmd_buffer[0] = 0;
    init_current_dir();
    strcpy(exec_path, "/SYSTEM32");
    show_prompt();
}

void NativeCmdHandleKeyEvent(const KEYBOARD_EVENT *event) {
    char c;

    if (!event || !event->pressed) return;

    switch (event->scancode) {
        case 0x0E:
            if (cmd_pos > 0) {
                cmd_pos--;
                HalPutChar('\b', 0x0F);
                HalPutChar(' ', 0x0F);
                HalPutChar('\b', 0x0F);
            }
            return;
        case 0x1C:
            process_command();
            return;
        case 0x01:
            cmd_pos = 0;
            cmd_buffer[0] = 0;
            show_prompt();
            return;
    }

    c = event->ascii;
    if (c && cmd_pos < 255) {
        cmd_buffer[cmd_pos++] = c;
        HalPutChar(c, 0x0F);
    }
}
