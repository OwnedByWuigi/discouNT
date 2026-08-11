#ifndef DISCOUNT_BUGCHECK_H
#define DISCOUNT_BUGCHECK_H

#include <stdint.h>

void KeBugCheckEx(uint32_t code, uint32_t p1, uint32_t p2,
                  uint32_t p3, uint32_t p4);
void KeBugCheck(uint32_t code);

#endif
