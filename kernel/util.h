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
#endif