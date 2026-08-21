#include <stdint.h>
#include "keyboard.h"
#include "arch/x86/portio.h"
#include "core/util.h"
#include "serial.h"

#define KBD_QUEUE_SIZE 32

static const char kbd_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char kbd_shift_ascii[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static KEYBOARD_EVENT kbd_queue[KBD_QUEUE_SIZE];
static uint32_t kbd_queue_head = 0;
static uint32_t kbd_queue_tail = 0;
static uint8_t kbd_shift = 0;
static uint8_t kbd_ctrl = 0;
static uint8_t kbd_alt = 0;
static uint8_t kbd_capslock = 0;
static uint8_t kbd_extended = 0;

static void kbd_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 1) return;
    }
}

static void kbd_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

static void kbd_drain_output(void) {
    uint32_t spins = 32;
    while (spins--) {
        if (!(inb(0x64) & 1)) break;
        (void)inb(0x60);
    }
}

static uint8_t kbd_command(uint8_t cmd) {
    kbd_wait_write();
    outb(0x60, cmd);
    kbd_wait_read();
    return inb(0x60);
}

static void kbd_queue_push(const KEYBOARD_EVENT *event) {
    uint32_t next = (kbd_queue_head + 1) % KBD_QUEUE_SIZE;
    if (next == kbd_queue_tail) return;
    kbd_queue[kbd_queue_head] = *event;
    kbd_queue_head = next;
}

static char kbd_translate_ascii(uint8_t scancode) {
    char c;

    if (scancode >= sizeof(kbd_ascii)) return 0;
    c = kbd_shift ? kbd_shift_ascii[scancode] : kbd_ascii[scancode];

    if (c >= 'a' && c <= 'z') {
        if (kbd_capslock ^ kbd_shift) c -= 32;
    } else if (c >= 'A' && c <= 'Z') {
        if (!(kbd_capslock ^ kbd_shift)) c += 32;
    }

    return c;
}

void KeyboardInit(void) {
    kbd_queue_head = 0;
    kbd_queue_tail = 0;
    kbd_shift = 0;
    kbd_ctrl = 0;
    kbd_alt = 0;
    kbd_capslock = 0;
    kbd_extended = 0;
    memset(kbd_queue, 0, sizeof(kbd_queue));

    kbd_drain_output();

    kbd_wait_write();
    outb(0x64, 0xAE);

    kbd_wait_write();
    outb(0x64, 0x20);
    kbd_wait_read();
    {
        uint8_t config = inb(0x60);
        config |= 0x01;
        config &= (uint8_t)~0x10;
        config |= 0x40;
        kbd_wait_write();
        outb(0x64, 0x60);
        kbd_wait_write();
        outb(0x60, config);
    }

    kbd_drain_output();

    {
        uint8_t ack = kbd_command(0xF4);
        SerialPutString("[KBD] Enable scanning ACK=");
        SerialPrintHex(ack);
        SerialPutString("\r\n");
    }
}

void KeyboardHandleData(uint8_t data) {
    uint8_t pressed;
    uint8_t scancode;
    KEYBOARD_EVENT event;

    if (data == 0xE0) {
        kbd_extended = 1;
        return;
    }

    pressed = ((data & 0x80) == 0);
    scancode = data & 0x7F;

    if (scancode == 0x2A || scancode == 0x36) kbd_shift = pressed ? 1 : 0;
    else if (scancode == 0x1D) kbd_ctrl = pressed ? 1 : 0;
    else if (scancode == 0x38) kbd_alt = pressed ? 1 : 0;
    else if (scancode == 0x3A && pressed) kbd_capslock ^= 1;

    event.scancode = scancode;
    event.ascii = pressed ? kbd_translate_ascii(scancode) : 0;
    event.pressed = pressed;
    event.shift = kbd_shift;
    event.ctrl = kbd_ctrl;
    event.alt = kbd_alt;
    event.extended = kbd_extended;

    /* Preserve E0-prefixed keys.  CSRSS needs E0 53 (Delete) for the
     * Ctrl+Alt+Delete secure-attention sequence; applications can ignore
     * extended events they do not understand. */
    kbd_queue_push(&event);

    kbd_extended = 0;
}

int KeyboardHandleControllerEvent(void) {
    uint8_t status;
    uint8_t data;

    status = inb(0x64);
    if (!(status & 1)) return 0;
    if (status & 0x20) return 0;

    data = inb(0x60);
    KeyboardHandleData(data);
    return 1;
}

int KeyboardPollEvent(KEYBOARD_EVENT *event) {
    if (!event) return 0;
    if (kbd_queue_head == kbd_queue_tail) return 0;

    *event = kbd_queue[kbd_queue_tail];
    kbd_queue_tail = (kbd_queue_tail + 1) % KBD_QUEUE_SIZE;
    return 1;
}
