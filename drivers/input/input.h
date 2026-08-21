#ifndef DISCOUNT_INPUT_H
#define DISCOUNT_INPUT_H
#include <stdint.h>
int InputReadControllerByte(int *is_mouse, uint8_t *data);
void InputInjectKeyboardByte(uint8_t data);
void InputInjectMouseByte(uint8_t data);
#endif
