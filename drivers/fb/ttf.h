#ifndef DRIVERS_FB_TTF_H
#define DRIVERS_FB_TTF_H

#include <stdint.h>

int FbTtfLoad(const char *path);
int FbTtfReady(void);
int FbTtfGlyph(char c, uint8_t rows[12]);

#endif
