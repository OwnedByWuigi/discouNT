#include <stdint.h>
#include "mouse.h"
#include "vga.h"
#include "fb.h"
#include "io/port.h"
#include "serial.h"

static MOUSE_STATE mouse = {320, 240, 0, 0, 0, 0};
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];
static uint8_t mouse_packet_size = 3;
static int mouse_old_x = 320, mouse_old_y = 240;
static MOUSE_CURSOR_TYPE mouse_cursor_type = MOUSE_CURSOR_ARROW;

// Arrow cursor (11x18 pixels)
// 1 = white pixel, 2 = black pixel (outline), 0 = transparent
static const uint8_t cursor[18][11] = {
    {1,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,1,2,2,1,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,1,1,0,0,0},
};

static const uint8_t cursor_sizewe[11][11] = {
    {0,0,0,0,0,2,0,0,0,0,0},
    {0,0,0,0,2,1,2,0,0,0,0},
    {0,0,0,2,1,1,1,2,0,0,0},
    {0,0,2,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,1,2},
    {2,1,1,1,1,1,1,1,1,1,2},
    {2,1,1,1,1,1,1,1,1,1,2},
    {0,0,2,1,1,1,1,1,2,0,0},
    {0,0,0,2,1,1,1,2,0,0,0},
    {0,0,0,0,2,1,2,0,0,0,0},
    {0,0,0,0,0,2,0,0,0,0,0},
};

static const uint8_t cursor_sizens[11][11] = {
    {0,0,0,0,2,2,2,0,0,0,0},
    {0,0,0,2,1,1,1,2,0,0,0},
    {0,0,2,1,1,1,1,1,2,0,0},
    {0,0,0,0,0,1,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,0,0},
    {0,0,2,1,1,1,1,1,2,0,0},
    {0,0,0,2,1,1,1,2,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
};

static const uint8_t cursor_sizenwse[11][11] = {
    {2,1,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0},
    {0,1,2,1,0,0,0,0,0,0,0},
    {0,0,1,2,1,0,0,0,0,0,0},
    {0,0,0,1,2,1,0,0,0,0,0},
    {0,0,0,0,1,2,1,0,0,0,0},
    {0,0,0,0,0,1,2,1,0,0,0},
    {0,0,0,0,0,0,1,2,1,0,0},
    {0,0,0,0,0,0,0,1,2,1,0},
    {0,0,0,0,0,0,0,0,1,2,1},
    {0,0,0,0,0,0,0,0,0,1,2},
};

static const uint8_t cursor_sizenesw[11][11] = {
    {0,0,0,0,0,0,0,0,0,1,2},
    {0,0,0,0,0,0,0,0,1,2,1},
    {0,0,0,0,0,0,0,1,2,1,0},
    {0,0,0,0,0,0,1,2,1,0,0},
    {0,0,0,0,0,1,2,1,0,0,0},
    {0,0,0,0,1,2,1,0,0,0,0},
    {0,0,0,1,2,1,0,0,0,0,0},
    {0,0,1,2,1,0,0,0,0,0,0},
    {0,1,2,1,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0},
    {2,1,0,0,0,0,0,0,0,0,0},
};

// Save background pixels under cursor
static uint8_t cursor_bg[18][11];
static int cursor_saved = 0;

static const uint8_t *mouse_cursor_at(int row, int col) {
    if (mouse_cursor_type == MOUSE_CURSOR_ARROW) return &cursor[row][col];
    if (row >= 11 || col >= 11) return &cursor[0][0];

    switch (mouse_cursor_type) {
        case MOUSE_CURSOR_SIZEWE:   return &cursor_sizewe[row][col];
        case MOUSE_CURSOR_SIZENS:   return &cursor_sizens[row][col];
        case MOUSE_CURSOR_SIZENWSE: return &cursor_sizenwse[row][col];
        case MOUSE_CURSOR_SIZENESW: return &cursor_sizenesw[row][col];
        default:                    return &cursor[row][col];
    }
}

static int mouse_cursor_width(void) {
    return (mouse_cursor_type == MOUSE_CURSOR_ARROW) ? 11 : 11;
}

static int mouse_cursor_height(void) {
    return (mouse_cursor_type == MOUSE_CURSOR_ARROW) ? 18 : 11;
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void MouseInit(void) {
    uint8_t status;
    
    SerialPutString("[Mouse] Init PS/2...\r\n");
    
    // Enable auxiliary device
    mouse_wait(1);
    outb(0x64, 0xA8);
    
    // Enable interrupts
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = inb(0x60) | 2;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    
    // Set defaults
    mouse_write(0xF6);
    mouse_read();
    
    /* IntelliMouse negotiation: USB/QEMU and most modern PS/2 devices
     * expose the wheel as a fourth packet byte. Keep a three-byte fallback. */
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80); mouse_read();
    mouse_write(0xF2);
    mouse_read();
    if (mouse_read() == 0x03) mouse_packet_size = 4;
    else mouse_packet_size = 3;

    /* Enable reporting only after the packet format has been selected. */
    mouse_write(0xF4);
    mouse_read();
    
    mouse.x = 320;
    mouse.y = 240;
    mouse_old_x = 320;
    mouse_old_y = 240;
    mouse.wheel_delta = 0;
    cursor_saved = 0;
    
    SerialPutString("[Mouse] Ready\r\n");
}

void MouseGetState(MOUSE_STATE *state) {
    state->x = mouse.x;
    state->y = mouse.y;
    state->buttons = mouse.buttons;
    state->left_down = mouse.left_down;
    state->right_down = mouse.right_down;
    state->middle_down = mouse.middle_down;
    state->wheel_delta = mouse.wheel_delta;
}

void MouseHandleByte(uint8_t data) {
    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) break;
            mouse.wheel_delta = 0;
            mouse_byte[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = data;
            mouse.buttons = mouse_byte[0] & 0x07;
            mouse.left_down = mouse.buttons & 1;
            mouse.right_down = mouse.buttons & 2;
            mouse.middle_down = mouse.buttons & 4;
            
            {
                int8_t x_move = mouse_byte[1];
                int8_t y_move = -mouse_byte[2];
                
                if (x_move > 0) x_move = x_move * 2;
                else if (x_move < 0) x_move = x_move * 2;
                if (y_move > 0) y_move = y_move * 2;
                else if (y_move < 0) y_move = y_move * 2;
                
                mouse.x += x_move;
                mouse.y += y_move;
            }
            
            {
                int max_x = FbGetWidth() - 12;
                int max_y = FbGetHeight() - 18;
                if (max_x < 0) max_x = 0;
                if (max_y < 0) max_y = 0;
                if (mouse.x < 0) mouse.x = 0;
                if (mouse.x > max_x) mouse.x = max_x;
                if (mouse.y < 0) mouse.y = 0;
                if (mouse.y > max_y) mouse.y = max_y;
            }
            if (mouse_packet_size == 3) mouse_cycle = 0;
            else mouse_cycle++;
            break;
        case 3:
            {
                int8_t wheel = data & 0x0F;
                if (wheel & 0x08) wheel -= 16;
                mouse.wheel_delta = wheel;
            }
            mouse_cycle = 0;
            break;
    }
}

void MouseHandleInterrupt(void) {
    uint8_t status = inb(0x64);
    
    if (!(status & 0x20)) return;
    if (!(status & 1)) return;
    
    MouseHandleByte(inb(0x60));
}

void MouseEraseCursor(void) {
    if (!cursor_saved) return;
    
    int x = mouse_old_x;
    int y = mouse_old_y;
    int width = mouse_cursor_width();
    int height = mouse_cursor_height();
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (*mouse_cursor_at(row, col) != 0) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < FbGetWidth() && py >= 0 && py < FbGetHeight()) {
                    FbPutPixel(px, py, cursor_bg[row][col]);
                }
            }
        }
    }
}

void MouseDrawCursor(void) {
    int x = mouse.x;
    int y = mouse.y;
    int width = mouse_cursor_width();
    int height = mouse_cursor_height();
    
    // Save background
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (*mouse_cursor_at(row, col) != 0) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < FbGetWidth() && py >= 0 && py < FbGetHeight()) {
                    cursor_bg[row][col] = FbGetPixel(px, py);
                }
            }
        }
    }
    cursor_saved = 1;
    
    // Draw cursor
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < FbGetWidth() && py >= 0 && py < FbGetHeight()) {
                if (*mouse_cursor_at(row, col) == 1) {
                    FbPutPixel(px, py, COLOR_WHITE);
                } else if (*mouse_cursor_at(row, col) == 2) {
                    FbPutPixel(px, py, COLOR_BLACK);
                }
            }
        }
    }
    
    mouse_old_x = mouse.x;
    mouse_old_y = mouse.y;
}

void MouseSetCursorType(MOUSE_CURSOR_TYPE type) {
    if (mouse_cursor_type == type) return;
    MouseEraseCursor();
    mouse_cursor_type = type;
    cursor_saved = 0;
}
