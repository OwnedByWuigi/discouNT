#ifndef DISCOUNT_HAL_H
#define DISCOUNT_HAL_H

#include <stdint.h>

void HalInitialize(void);
void HalConfigureBootDisplay(void *boot_info);
void HalClearScreen(uint8_t color);
void HalPutChar(char c, uint8_t color);
void HalPutString(const char *str, uint8_t color);
void HalSetCursor(int x, int y);
void HalGetCursor(int *x, int *y);
uint8_t HalGetAttribute(void);

#endif
