#ifndef DISCOUNT_FCNTL_H
#define DISCOUNT_FCNTL_H
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x40
#define O_TRUNC 0x200
int open(const char *, int, ...); int close(int); int read(int, void *, unsigned int); int write(int, const void *, unsigned int);
#endif
