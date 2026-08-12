#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

typedef struct _KEYBOARD_EVENT {
    uint8_t scancode;
    char ascii;
    uint8_t pressed;
    uint8_t shift;
    uint8_t ctrl;
    uint8_t alt;
    uint8_t extended;
} KEYBOARD_EVENT;

void KeyboardInit(void);
void KeyboardHandleData(uint8_t data);
int KeyboardHandleControllerEvent(void);
int KeyboardPollEvent(KEYBOARD_EVENT *event);

#endif
