#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>

#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4

typedef enum {
    MOUSE_CURSOR_ARROW = 0,
    MOUSE_CURSOR_SIZEWE,
    MOUSE_CURSOR_SIZENS,
    MOUSE_CURSOR_SIZENWSE,
    MOUSE_CURSOR_SIZENESW
} MOUSE_CURSOR_TYPE;

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
void MouseSetCursorType(MOUSE_CURSOR_TYPE type);
void MouseHandleByte(uint8_t data);
void MouseHandleInterrupt(void);
#endif
