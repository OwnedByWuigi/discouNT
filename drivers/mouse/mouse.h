#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>

#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4

typedef struct {
    int x, y;
    uint8_t buttons;
    uint8_t left_down;
    uint8_t right_down;
    uint8_t middle_down;
} MOUSE_STATE;

void MouseInit(void);
void MouseGetState(MOUSE_STATE *state);
void MouseDrawCursor(void);
void MouseEraseCursor(void);
void MouseHandleByte(uint8_t data);
void MouseHandleInterrupt(void);
#endif
