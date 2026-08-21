#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>

void *memset(void *s, int c, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);
uint32_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void strcpy(char *d, const char *s);
void strcat(char *d, const char *s);
void itoa(int val, char *buf, int base);
uint64_t __udivdi3(uint64_t num, uint64_t den);
uint64_t __umoddi3(uint64_t num, uint64_t den);
int64_t __divdi3(int64_t num, int64_t den);
int64_t __moddi3(int64_t num, int64_t den);
#endif
