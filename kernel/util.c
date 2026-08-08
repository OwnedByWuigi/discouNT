// kernel/util.c
#include <stdint.h>
#include <stddef.h>

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    while (n--) *p++ = c;
    return s;
}

void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *dst = d;
    const uint8_t *src = s;
    while (n--) *dst++ = *src++;
    return d;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

void itoa(int val, char *buf, int base) {
    char tmp[16];
    int i = 0, j;
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (val) {
        int r = val % base;
        tmp[i++] = r < 10 ? '0' + r : 'A' + r - 10;
        val /= base;
    }
    j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}