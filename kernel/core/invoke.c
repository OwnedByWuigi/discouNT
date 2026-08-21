#include <stdint.h>
#include "core/invoke.h"

int KeInvokeMain(void *entry, void *stack, uint32_t stack_size) {
    typedef int (*ENTRY)(void);
    (void)stack;
    (void)stack_size;
    return entry ? ((ENTRY)entry)() : -1;
}

int KeInvokeMainArgs(void *entry, const char *image_path, const char *command_line,
                     void *stack, uint32_t stack_size) {
    typedef int (*ENTRY)(int, char **);
    char buffer[256];
    char *argv[32];
    int argc = 1;
    int i = 0;
    (void)stack;
    (void)stack_size;
    if (!entry) return -1;
    argv[0] = (char *)(image_path ? image_path : "");
    while (command_line && command_line[i] && i < (int)sizeof(buffer) - 1) {
        buffer[i] = command_line[i];
        i++;
    }
    buffer[i] = 0;
    i = 0;
    while (buffer[i] && argc < 31) {
        char quote = 0;
        while (buffer[i] == ' ' || buffer[i] == '\t') i++;
        if (!buffer[i]) break;
        if (buffer[i] == '\"' || buffer[i] == '\'') quote = buffer[i++];
        argv[argc++] = &buffer[i];
        while (buffer[i] && ((quote && buffer[i] != quote) ||
                             (!quote && buffer[i] != ' ' && buffer[i] != '\t'))) i++;
        if (buffer[i]) buffer[i++] = 0;
    }
    argv[argc] = 0;
    return ((ENTRY)entry)(argc, argv);
}

int KeInvokeWinMain(void *entry, void *image, const char *command_line,
                    void *stack, uint32_t stack_size) {
    typedef int (*ENTRY)(void *, void *, char *, int);
    static char empty_command_line[1];
    (void)stack;
    (void)stack_size;
    return entry ? ((ENTRY)entry)(image, 0,
                                  (char *)(command_line ? command_line : empty_command_line),
                                  1) : -1;
}

int KeInvokeWWinMain(void *entry, void *image, const uint32_t *command_line,
                     void *stack, uint32_t stack_size) {
    /* Native discouNT ELF userland is currently compiled with the toolchain's
       32-bit wchar_t ABI.  Feeding a UTF-16 buffer here combines adjacent
       characters and makes switches such as /desktop unrecognisable. */
    typedef int (*ENTRY)(void *, void *, uint32_t *, int);
    static uint32_t empty_command_line[1];
    (void)stack;
    (void)stack_size;
    return entry ? ((ENTRY)entry)(image, 0,
                                  (uint32_t *)(command_line ? command_line : empty_command_line),
                                  1) : -1;
}
