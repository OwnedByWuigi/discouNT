#ifndef RTLPATH_H
#define RTLPATH_H
#include <stdint.h>

int RtlNormalizePath(const char *path, char *output, uint32_t capacity);
int RtlNextPathComponent(const char **cursor, char *component, uint32_t capacity);
int RtlJoinPath(const char *base, const char *name, char *output, uint32_t capacity);
const char *RtlPathFileName(const char *path);
int RtlReplacePathExtension(const char *path, const char *extension,
                            char *output, uint32_t capacity);

#endif
