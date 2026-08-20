#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>
int Fat32Initialize(const char *device_name);
int Fat32IsMounted(void);
int Fat32ReadFile(const char *path, uint8_t **buffer, uint32_t *size);
#endif
