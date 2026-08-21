#include <stdint.h>
#include "core/util.h"

static uint64_t util_udivmod64(uint64_t num, uint64_t den, uint64_t *rem_out) {
    uint64_t q = 0;
    uint64_t r = 0;
    int i;

    if (den == 0) {
        if (rem_out) *rem_out = 0;
        return 0;
    }

    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((num >> i) & 1ULL);
        if (r >= den) {
            r -= den;
            q |= (1ULL << i);
        }
    }

    if (rem_out) *rem_out = r;
    return q;
}

void *memset(void *s, int c, uint32_t n) {
    uint8_t *p = s;
    while (n--) *p++ = c;
    return s;
}

void *memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *dst = d;
    const uint8_t *src = s;
    while (n--) *dst++ = *src++;
    return d;
}

void *memmove(void *d, const void *s, uint32_t n) {
    uint8_t *dst = (uint8_t *)d;
    const uint8_t *src = (const uint8_t *)s;
    if (dst == src || !n) return d;
    if (dst < src || dst >= src + n) {
        for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
    } else {
        while (n) { n--; dst[n] = src[n]; }
    }
    return d;
}

uint32_t wcstoul(const uint16_t *text, uint16_t **end, int base) {
    uint32_t value = 0, digit;
    const uint16_t *p = text;
    if (!p) { if (end) *end = 0; return 0; }
    while (*p == ' ' || *p == '\t') p++;
    if (!base) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
        else base = p[0] == '0' ? 8 : 10;
    }
    while (*p) {
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (*p >= 'a' && *p <= 'z') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') digit = *p - 'A' + 10;
        else break;
        if (digit >= (uint32_t)base) break;
        value = value * (uint32_t)base + digit;
        p++;
    }
    if (end) *end = (uint16_t *)p;
    return value;
}

uint32_t strlen(const char *s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

void strcpy(char *d, const char *s) {
    while ((*d++ = *s++));
}

void strcat(char *d, const char *s) {
    while (*d) d++;
    while ((*d++ = *s++));
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

uint64_t __udivdi3(uint64_t num, uint64_t den) {
    return util_udivmod64(num, den, 0);
}

uint64_t __umoddi3(uint64_t num, uint64_t den) {
    uint64_t rem = 0;
    util_udivmod64(num, den, &rem);
    return rem;
}

int64_t __divdi3(int64_t num, int64_t den) {
    uint64_t un;
    uint64_t ud;
    uint64_t uq;
    int neg = 0;

    if (den == 0) return 0;

    if (num < 0) {
        un = (uint64_t)(-num);
        neg ^= 1;
    } else {
        un = (uint64_t)num;
    }

    if (den < 0) {
        ud = (uint64_t)(-den);
        neg ^= 1;
    } else {
        ud = (uint64_t)den;
    }

    uq = util_udivmod64(un, ud, 0);
    return neg ? -(int64_t)uq : (int64_t)uq;
}

int64_t __moddi3(int64_t num, int64_t den) {
    uint64_t un;
    uint64_t ud;
    uint64_t rem = 0;
    int neg = 0;

    if (den == 0) return 0;

    if (num < 0) {
        un = (uint64_t)(-num);
        neg = 1;
    } else {
        un = (uint64_t)num;
    }

    if (den < 0) ud = (uint64_t)(-den);
    else ud = (uint64_t)den;

    util_udivmod64(un, ud, &rem);
    return neg ? -(int64_t)rem : (int64_t)rem;
}
