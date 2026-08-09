#include <stdint.h>
#include "hal.h"
#include "mm.h"
#include "object.h"
#include "ke.h"
#include "portio.h"
#include "util.h"
#include "serial.h"
#include "cdfs.h"
#include "peloader.h"
#include "vga.h"
#include "fb.h"
#include "win32k.h"
#include "mouse.h"

static const char scancode_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static char cmd_buffer[256];
static int cmd_pos = 0;
static int shift_pressed = 0;
static int capslock = 0;

// Current directory state
static uint32_t current_dir_lba = 0;
static uint32_t current_dir_size = 0;
static char current_path[256] = "/";

// Initialize current directory to root
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

// Find an entry in a directory
static int find_in_dir(uint32_t dir_lba, uint32_t dir_size, const char *name,
                        uint32_t *out_lba, uint32_t *out_size, int *is_dir) {
    uint32_t sectors = (dir_size + 2047) / 2048;
    uint8_t *dir = (uint8_t*)kmalloc(sectors * 2048);
    if (!dir) return 0;
    
    for (uint32_t i = 0; i < sectors; i++)
        CdfsReadSector(dir_lba + i, dir + i * 2048);
    
    int off = 0, found = 0;
    
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

static void cmd_help(void) {
    HalPutString("\nCommands:\n", 0x0F);
    HalPutString("  help    - Show help\n", 0x0F);
    HalPutString("  clear   - Clear screen\n", 0x0F);
    HalPutString("  info    - System info\n", 0x0F);
    HalPutString("  ls/dir  - List files\n", 0x0F);
    HalPutString("  cd <dir> - Change directory\n", 0x0F);
    HalPutString("  cd ..   - Go up one level\n", 0x0F);
    HalPutString("  cd /    - Go to root\n", 0x0F);
    HalPutString("  exec <file> - Load/run a file\n", 0x0F);
    HalPutString("  pwd     - Print working directory\n", 0x0F);
    HalPutString("  reboot  - Reboot\n", 0x0F);
}

static void cmd_clear(void) {
    HalClearScreen(0x1F);
}

static void cmd_info(void) {
    HalPutString("\nNT-like OS v0.9\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Kernel: NT-like\n", 0x0F);
    HalPutString("Filesystem: CDFS (ISO 9660)\n", 0x0F);
    HalPutString("Display: VGA Text 80x25\n", 0x0F);
}

static void cmd_pwd(void) {
    HalPutString("\n", 0x0F);
    HalPutString(current_path, 0x0F);
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
    
    uint32_t sectors = (current_dir_size + 2047) / 2048;
    uint8_t *dir = (uint8_t*)kmalloc(sectors * 2048);
    if (!dir) { HalPutString("Out of memory\n", 0x0C); return; }
    
    for (uint32_t i = 0; i < sectors; i++)
        CdfsReadSector(current_dir_lba + i, dir + i * 2048);
    
    int off = 0, count = 0;
    char buf[16];
    
    // Show . and .. first
    HalPutString("  <DIR>   .\n", 0x0E);
    HalPutString("  <DIR>   ..\n", 0x0E);
    count = 2;
    
    while (off < (int)(sectors * 2048)) {
        uint8_t len = dir[off];
        if (len == 0) break;
        
        uint8_t flags = dir[off + 25];
        uint8_t name_len = dir[off + 32];
        char *name = (char*)(dir + off + 33);
        uint32_t fsize = *(uint32_t*)(dir + off + 10);
        
        if (flags & 0x02) {
            HalPutString("  <DIR>   ", 0x0E);
        } else {
            HalPutString("         ", 0x0F);
        }
        
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

static void cmd_cd(char *args) {
    if (!args || !*args) {
        cmd_pwd();
        return;
    }
    
    // Uppercase
    char dirname[256];
    int dlen = strlen(args);
    if (dlen > 254) dlen = 254;
    for (int i = 0; i < dlen; i++) {
        dirname[i] = args[i];
        if (dirname[i] >= 'a' && dirname[i] <= 'z') dirname[i] -= 32;
    }
    dirname[dlen] = 0;
    
    // cd /
    if (strcmp(dirname, "/") == 0) {
        init_current_dir();
        HalPutString("\nChanged to root\n", 0x0A);
        return;
    }
    
    // cd ..
    if (strcmp(dirname, "..") == 0) {
        if (current_path[1] == 0) {
            HalPutString("\nAlready at root\n", 0x0C);
            return;
        }
        
        // Read root to get parent
        uint8_t sector[2048];
        if (!CdfsReadSector(16, sector)) return;
        uint8_t *root = sector + 156;
        uint32_t root_lba = *(uint32_t*)(root + 2);
        uint32_t root_size = *(uint32_t*)(root + 10);
        
        // If only one level deep, go to root
        int slash_count = 0;
        for (int i = 0; current_path[i]; i++)
            if (current_path[i] == '/') slash_count++;
        
        if (slash_count <= 1) {
            current_dir_lba = root_lba;
            current_dir_size = root_size;
            current_path[0] = '/';
            current_path[1] = 0;
        } else {
            // Navigate from root to parent
            // Build parent path
            int last_slash = 0;
            for (int i = 0; current_path[i]; i++)
                if (current_path[i] == '/' && i > 0) last_slash = i;
            
            current_path[last_slash] = 0;
            
            // Re-navigate from root
            current_dir_lba = root_lba;
            current_dir_size = root_size;
            
            char *part = current_path + 1; // Skip first /
            while (part && *part) {
                char *next = 0;
                for (int i = 0; part[i]; i++) {
                    if (part[i] == '/') { part[i] = 0; next = part + i + 1; break; }
                }
                
                uint32_t new_lba, new_size;
                int is_dir;
                if (find_in_dir(current_dir_lba, current_dir_size, part, &new_lba, &new_size, &is_dir) && is_dir) {
                    current_dir_lba = new_lba;
                    current_dir_size = new_size;
                }
                part = next;
            }
            
            // Restore path
            current_path[last_slash] = '/';
            current_path[last_slash] = 0;
        }
        
        HalPutString("\n", 0x0A);
        HalPutString(current_path, 0x0F);
        HalPutString("\n", 0x0A);
        return;
    }
    
    // cd <subdir>
    uint32_t new_lba, new_size;
    int is_dir;
    
    if (find_in_dir(current_dir_lba, current_dir_size, dirname, &new_lba, &new_size, &is_dir)) {
        if (is_dir) {
            current_dir_lba = new_lba;
            current_dir_size = new_size;
            
            // Update path
            int plen = strlen(current_path);
            if (current_path[plen - 1] != '/') {
                current_path[plen] = '/';
                current_path[plen + 1] = 0;
            }
            strcat(current_path, dirname);
            
            HalPutString("\nChanged to ", 0x0A);
            HalPutString(current_path, 0x0F);
            HalPutString("\n", 0x0A);
        } else {
            HalPutString("\nNot a directory: ", 0x0C);
            HalPutString(dirname, 0x0C);
            HalPutString("\n", 0x0C);
        }
    } else {
        HalPutString("\nDirectory not found: ", 0x0C);
        HalPutString(dirname, 0x0C);
        HalPutString("\n", 0x0C);
    }
}

static void cmd_exec(char *args) {
    if (!args || !*args) {
        HalPutString("\nUsage: exec <filename>\n", 0x0C);
        return;
    }
    
    char filename[256];
    int flen = strlen(args);
    if (flen > 254) flen = 254;
    for (int i = 0; i < flen; i++) {
        filename[i] = args[i];
        if (filename[i] >= 'a' && filename[i] <= 'z') filename[i] -= 32;
    }
    filename[flen] = 0;
    
    HalPutString("\nLoading: ", 0x0F);
    HalPutString(filename, 0x0F);
    HalPutString("...\n", 0x0F);
    
    uint32_t file_lba, file_size;
    int is_dir;
    
    if (!find_in_dir(current_dir_lba, current_dir_size, filename, &file_lba, &file_size, &is_dir)) {
        HalPutString("File not found\n", 0x0C);
        return;
    }
    
    if (is_dir) { HalPutString("Is a directory\n", 0x0C); return; }
    
    uint8_t *file_buf = (uint8_t*)kmalloc(file_size);
    if (!file_buf) { HalPutString("Out of memory\n", 0x0C); return; }
    
    uint32_t sectors = (file_size + 2047) / 2048;
    for (uint32_t i = 0; i < sectors; i++)
        CdfsReadSector(file_lba + i, file_buf + i * 2048);
    
    // Validate PE
    if (file_size < 64 || file_buf[0] != 0x4D || file_buf[1] != 0x5A) {
        HalPutString("Not a valid PE file\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    uint32_t pe_off = *(uint32_t*)(file_buf + 0x3C);
    if (pe_off + 4 > file_size || file_buf[pe_off] != 'P' || file_buf[pe_off+1] != 'E') {
        HalPutString("Invalid PE header\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    uint16_t machine = *(uint16_t*)(file_buf + pe_off + 4);
    uint8_t *opt = file_buf + pe_off + 24;
    uint16_t opt_magic = *(uint16_t*)(opt);
    
    if (machine != 0x14C) {
        HalPutString("Not x86 executable\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    if (opt_magic != 0x10B) {
        HalPutString("Not 32-bit PE\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    uint16_t subsystem = *(uint16_t*)(opt + 68);
    uint32_t entry_rva = *(uint32_t*)(opt + 16);
    
    const char *subsys[] = {"Unknown","Native","GUI","Console","OS/2","POSIX","","WinCE"};
    
    HalPutString("\nPE Info: ", 0x0F);
    if (subsystem < 8) HalPutString(subsys[subsystem], 0x0A);
    HalPutString(" | Entry: 0x", 0x0F);
    char buf[16];
    itoa(entry_rva, buf, 16);
    HalPutString(buf, 0x0F);
    HalPutString("\n", 0x0F);
    
    // GUI apps need graphics mode - handle separately
    if (subsystem == 2) {
        HalPutString("\n*** GUI Application ***\n", 0x0E);
        HalPutString("GUI apps require graphics mode.\n", 0x0F);
        HalPutString("Use 'gui' command to enter GUI first.\n", 0x0F);
        kfree(file_buf);
        return;
    }
    
    // Native (1), Console (3), POSIX (5) - run in text mode
    if (subsystem != 1 && subsystem != 3 && subsystem != 5) {
        HalPutString("Unsupported subsystem: ", 0x0C);
        itoa(subsystem, buf, 10);
        HalPutString(buf, 0x0C);
        HalPutString("\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    // Load PE
    HalPutString("Mapping PE into memory...\n", 0x0F);
    void *image = PeLoadImage(file_buf, file_size);
    
    if (!image) {
        HalPutString("ERROR: Failed to load PE image!\n", 0x0C);
        HalPutString("Check serial for details.\n", 0x0C);
        kfree(file_buf);
        return;
    }
    
    HalPutString("Resolving imports...\n", 0x0F);
    PeResolveImports(image);
    
    HalPutString("Applying relocations...\n", 0x0F);
    PePerformRelocations(image);
    
    void *entry = PeGetEntryPoint(image);
    SerialPutString("[PE] Entry: 0x");
    SerialPrintHex((uint32_t)entry);
    SerialPutString("\r\n");
    
    // Execute in text mode with separate stack
    HalPutString("Executing...\n", 0x0A);
    HalPutString("================================\n", 0x0F);
    
    uint8_t *exe_stack = (uint8_t*)kmalloc(65536);
    uint32_t exe_esp = (uint32_t)(exe_stack + 65536 - 256);
    
    uint32_t kernel_esp;
    __asm__ volatile("movl %%esp, %0" : "=r"(kernel_esp));
    
    typedef int (*EntryFunc)(void);
    EntryFunc func = (EntryFunc)entry;
    int ret = 0;
    
    __asm__ volatile(
        "movl %%esp, %[save]\n"
        "movl %[newsp], %%esp\n"
        "call *%[fn]\n"
        "movl %%eax, %[retval]\n"
        "movl %[save], %%esp\n"
        : [retval] "=r"(ret),
          [save] "=m"(*(uint32_t*)0)
        : [newsp] "r"(exe_esp),
          [fn] "r"(func)
        : "eax", "ecx", "edx", "memory"
    );
    
    kfree(exe_stack);
    
    HalPutString("\n================================", 0x0F);
    HalPutString("\n\nProgram exited with code: ", 0x0A);
    itoa(ret, buf, 10);
    HalPutString(buf, 0x0A);
    HalPutString("\n", 0x0A);
    
    kfree(image);
    kfree(file_buf);
}

static void cmd_reboot(void) {
    HalPutString("\nRebooting...\n", 0x0C);
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile("int $0");
}

static void process_command(void) {
    cmd_buffer[cmd_pos] = 0;
    HalPutString("\n", 0x0F);
    SerialPutString("[CMD] ");
    SerialPutString(cmd_buffer);
    SerialPutString("\r\n");
    
    char *cmd = cmd_buffer;
    while (*cmd == ' ') cmd++;
    
    char *args = 0;
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
    else if (strcmp(cmd, "exec") == 0) cmd_exec(args);
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (*cmd) {
        HalPutString("Unknown: ", 0x0C);
        HalPutString(cmd, 0x0C);
        HalPutString("\nType 'help' for commands\n", 0x0C);
    }
    
    cmd_pos = 0;
    cmd_buffer[0] = 0;
    
    // Show prompt with current path
    HalPutString("\n", 0x0F);
    HalPutString(current_path, 0x1F);
    HalPutString("> ", 0x1F);
}

static void handle_keyboard(uint8_t scancode) {
    if (scancode & 0x80) {
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) shift_pressed = 0;
        return;
    }
    
    switch (scancode) {
        case 0x2A: case 0x36: shift_pressed = 1; return;
        case 0x3A: capslock = !capslock; return;
        case 0x0E:
            if (cmd_pos > 0) { cmd_pos--; HalPutChar('\b', 0x0F); HalPutChar(' ', 0x0F); HalPutChar('\b', 0x0F); }
            return;
        case 0x1C: process_command(); return;
        case 0x01: cmd_pos = 0; HalPutString("\n", 0x0F); HalPutString(current_path, 0x1F); HalPutString("> ", 0x1F); return;
    }
    
    if (scancode >= sizeof(scancode_ascii)) return;
    char c = shift_pressed ? scancode_shift[scancode] : scancode_ascii[scancode];
    if (capslock && c >= 'a' && c <= 'z') c -= 32;
    if (capslock && shift_pressed && c >= 'A' && c <= 'Z') c += 32;
    
    if (c && cmd_pos < 255) { cmd_buffer[cmd_pos++] = c; HalPutChar(c, 0x0F); }
}

void kmain(uint32_t magic, void *mb_info_ptr) {
    (void)magic; (void)mb_info_ptr;
    
    SerialInit();
    SerialPutString("\r\n========================================\r\n");
    SerialPutString("  NT-like OS v0.9\r\n");
    SerialPutString("========================================\r\n\r\n");
    
    HalInitialize();
    HalClearScreen(0x1F);
    HalPutString("NT-like OS v0.9\n", 0x1F);
    HalPutString("===============\n", 0x1F);
    HalPutString("Type 'help' for commands\n\n", 0x0F);
    
    ObInit();
    KeInit();
    CdfsInit();
    init_current_dir();
    
    HalPutString(current_path, 0x1F);
    HalPutString("> ", 0x1F);
    
    while(1) {
        if (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            uint8_t data = inb(0x60);
            if (!(status & 0x20) && (status & 1)) handle_keyboard(data);
        }
        for (volatile int i = 0; i < 5000; i++);
    }
}