#include <stdint.h>
#include "rtl/rtlpath.h"

static int RtlIsPathSeparator(char ch) { return ch == '/' || ch == '\\'; }

int RtlNextPathComponent(const char **cursor, char *component, uint32_t capacity) {
    const char *path;
    uint32_t length = 0;
    if (!cursor || !*cursor || !component || capacity == 0) return -1;
    path = *cursor;
    while (RtlIsPathSeparator(*path)) path++;
    if (!*path) {
        component[0] = 0;
        *cursor = path;
        return 0;
    }
    while (*path && !RtlIsPathSeparator(*path)) {
        if (length + 1 >= capacity) return -1;
        component[length++] = *path++;
    }
    component[length] = 0;
    while (RtlIsPathSeparator(*path)) path++;
    *cursor = path;
    return 1;
}

int RtlNormalizePath(const char *path, char *output, uint32_t capacity) {
    const char *cursor;
    char component[256];
    uint32_t length = 0;
    int absolute;
    int result;
    if (!path || !output || capacity < 2) return -1;
    absolute = RtlIsPathSeparator(path[0]);
    cursor = path;
    if (absolute) output[length++] = '/';
    output[length] = 0;

    while ((result = RtlNextPathComponent(&cursor, component, sizeof(component))) > 0) {
        uint32_t component_length = 0;
        if (component[0] == '.' && component[1] == 0) continue;
        if (component[0] == '.' && component[1] == '.' && component[2] == 0) {
            uint32_t floor = absolute ? 1 : 0;
            if (length > floor) {
                if (length > floor && output[length - 1] == '/') length--;
                while (length > floor && output[length - 1] != '/') length--;
                if (length > floor && output[length - 1] == '/') length--;
                output[length] = 0;
                continue;
            }
            if (absolute) continue;
        }
        while (component[component_length]) component_length++;
        if (length && output[length - 1] != '/') {
            if (length + 1 >= capacity) return -1;
            output[length++] = '/';
        }
        if (length + component_length >= capacity) return -1;
        for (uint32_t i = 0; i < component_length; i++)
            output[length++] = component[i];
        output[length] = 0;
    }
    if (result < 0) return -1;
    if (length == 0) {
        output[0] = absolute ? '/' : '.';
        output[1] = 0;
        length = 1;
    }
    return (int)length;
}

int RtlJoinPath(const char *base, const char *name, char *output, uint32_t capacity) {
    char combined[512];
    uint32_t length = 0;
    if (!base || !name || !output || !capacity) return -1;
    if (RtlIsPathSeparator(name[0])) return RtlNormalizePath(name, output, capacity);
    while (base[length] && length + 1 < sizeof(combined)) {
        combined[length] = base[length];
        length++;
    }
    if (base[length]) return -1;
    if (length && !RtlIsPathSeparator(combined[length - 1])) {
        if (length + 1 >= sizeof(combined)) return -1;
        combined[length++] = '/';
    }
    for (uint32_t i = 0; name[i]; i++) {
        if (length + 1 >= sizeof(combined)) return -1;
        combined[length++] = name[i];
    }
    combined[length] = 0;
    return RtlNormalizePath(combined, output, capacity);
}

const char *RtlPathFileName(const char *path) {
    const char *name = path;
    if (!path) return 0;
    while (*path) {
        if (RtlIsPathSeparator(*path)) name = path + 1;
        path++;
    }
    return name;
}

int RtlReplacePathExtension(const char *path, const char *extension,
                            char *output, uint32_t capacity) {
    const char *name;
    uint32_t dot = 0, length = 0, extension_length = 0;
    if (!path || !extension || !output || !capacity) return -1;
    name = RtlPathFileName(path);
    while (path[length]) {
        if (path + length >= name && path[length] == '.') dot = length;
        length++;
    }
    if (!dot || path + dot < name) dot = length;
    while (extension[extension_length]) extension_length++;
    if (dot + extension_length + 1 > capacity) return -1;
    for (uint32_t i = 0; i < dot; i++) output[i] = path[i];
    for (uint32_t i = 0; i < extension_length; i++) output[dot + i] = extension[i];
    output[dot + extension_length] = 0;
    return (int)(dot + extension_length);
}
