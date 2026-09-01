#include <stdint.h>
#include "io/driver.h"
#include "loader/peloader.h"
#include "serial.h"
#include "vga.h"
#include "cdfs.h"
#include "fat32.h"
#include "keyboard.h"
#include "mouse.h"
#include "net.h"
#include "fb.h"
#include "w32k.h"
#include "ob/object.h"

void BootSerialInit(void);
void BootSerialSetDebugEnabled(int enabled);
int BootSerialIsDebugEnabled(void);
void BootSerialPutChar(char c);
void BootSerialPutString(const char *str);
void BootSerialPrintHex(uint32_t val);
void BootSerialPrintDec(uint32_t val);

void BootCdfsInit(void);
int BootCdfsReadSector(uint32_t lba, uint8_t *buffer);
int BootCdfsFindFile(const char *path, uint32_t *out_lba, uint32_t *out_size);
int BootCdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);

static void (*pSerialInit)(void) = 0;
static void (*pSerialPutChar)(char c) = 0;
static void (*pSerialPutString)(const char *str) = 0;
static void (*pSerialPrintHex)(uint32_t val) = 0;
static void (*pSerialPrintDec)(uint32_t val) = 0;
static int serial_debug_enabled = 0;
static int screen_debug_enabled = 0;
static int screen_debug_x = 0;
static int screen_debug_y = 0;

#define SCREEN_DEBUG_WIDTH 80
#define SCREEN_DEBUG_HEIGHT 25
#define SCREEN_DEBUG_MEMORY ((volatile uint16_t*)0xB8000)

static void ScreenDebugPutChar(char c) {
    int x, y;
    if (c == '\r') {
        screen_debug_x = 0;
        return;
    }
    if (c == '\n') {
        screen_debug_x = 0;
        screen_debug_y++;
    } else {
        SCREEN_DEBUG_MEMORY[screen_debug_y * SCREEN_DEBUG_WIDTH + screen_debug_x] =
            (uint16_t)(uint8_t)c | 0x0700;
        if (++screen_debug_x >= SCREEN_DEBUG_WIDTH) {
            screen_debug_x = 0;
            screen_debug_y++;
        }
    }
    if (screen_debug_y < SCREEN_DEBUG_HEIGHT) return;
    for (y = 1; y < SCREEN_DEBUG_HEIGHT; y++)
        for (x = 0; x < SCREEN_DEBUG_WIDTH; x++)
            SCREEN_DEBUG_MEMORY[(y - 1) * SCREEN_DEBUG_WIDTH + x] =
                SCREEN_DEBUG_MEMORY[y * SCREEN_DEBUG_WIDTH + x];
    for (x = 0; x < SCREEN_DEBUG_WIDTH; x++)
        SCREEN_DEBUG_MEMORY[(SCREEN_DEBUG_HEIGHT - 1) * SCREEN_DEBUG_WIDTH + x] = 0x0720;
    screen_debug_y = SCREEN_DEBUG_HEIGHT - 1;
}

static void (*pVgaInit)(void) = 0;
static void (*pVgaClearScreen)(uint8_t color) = 0;
static void (*pVgaPutPixel)(int x, int y, uint8_t color) = 0;
static void (*pVgaFillRect)(int x, int y, int w, int h, uint8_t color) = 0;
static void (*pVgaDrawRect)(int x, int y, int w, int h, uint8_t color) = 0;
static void (*pVgaDrawChar)(int x, int y, char c, uint8_t fg, uint8_t bg) = 0;
static void (*pVgaDrawString)(int x, int y, const char *str, uint8_t fg, uint8_t bg) = 0;
static void (*pVgaSwapBuffers)(void) = 0;

static void (*pCdfsInit)(void) = 0;
static int (*pCdfsReadSector)(uint32_t lba, uint8_t *buffer) = 0;
static int (*pCdfsFindFile)(const char *path, uint32_t *out_lba, uint32_t *out_size) = 0;
static int (*pCdfsReadFile)(const char *path, uint8_t **out_buffer, uint32_t *out_size) = 0;

static void (*pKeyboardInit)(void) = 0;
static void (*pKeyboardHandleData)(uint8_t data) = 0;
static int (*pKeyboardHandleControllerEvent)(void) = 0;
static int (*pKeyboardPollEvent)(KEYBOARD_EVENT *event) = 0;

static void (*pMouseInit)(void) = 0;
static void (*pMouseGetState)(MOUSE_STATE *state) = 0;
static void (*pMouseDrawCursor)(void) = 0;
static void (*pMouseEraseCursor)(void) = 0;
static void (*pMouseSetCursorType)(MOUSE_CURSOR_TYPE type) = 0;
static void (*pMouseHandleByte)(uint8_t data) = 0;
static void (*pMouseHandleInterrupt)(void) = 0;

static void (*pNetInit)(void) = 0;
static void (*pNetPoll)(void) = 0;
static int (*pNetIsReady)(void) = 0;
static int (*pNetPing)(const char *ip_text, char *out_text, int out_text_len) = 0;
static int (*pNetResolve)(const char *name, char *out_ip, int out_ip_len) = 0;

static void (*pFbInit)(void *multiboot_info) = 0;
static void (*pFbClearScreen)(uint8_t color) = 0;
static void (*pFbPutPixel)(int x, int y, uint8_t color) = 0;
static void (*pFbFillRect)(int x, int y, int w, int h, uint8_t color) = 0;
static void (*pFbFillRectRGB)(int x, int y, int w, int h, uint32_t rgb) = 0;
static void (*pFbDrawRect)(int x, int y, int w, int h, uint8_t color) = 0;
static void (*pFbDrawChar)(int x, int y, char c, uint8_t fg, uint8_t bg) = 0;
static void (*pFbDrawString)(int x, int y, const char *str, uint8_t fg, uint8_t bg) = 0;
static void (*pFbDrawCharTransparent)(int x, int y, char c, uint8_t fg) = 0;
static void (*pFbDrawStringTransparent)(int x, int y, const char *str, uint8_t fg) = 0;
static uint32_t (*pFbGetPixelRGB)(int x, int y) = 0;
static void (*pFbPutPixelRGB)(int x, int y, uint32_t rgb) = 0;
static int (*pFbPaintWallpaper)(int x, int y, int w, int h, const char *path) = 0;
static void (*pFbCaptureRGB)(int x, int y, int w, int h, uint32_t *dst, int dst_stride) = 0;
static void (*pFbBlitRGB)(int x, int y, int w, int h, const uint32_t *src, int src_stride) = 0;
static void (*pFbSetClipRect)(int x, int y, int w, int h) = 0;
static void (*pFbResetClipRect)(void) = 0;
static void (*pFbSwapBuffers)(void) = 0;
static int (*pFbIsFramebuffer)(void) = 0;
static int (*pFbGetWidth)(void) = 0;
static int (*pFbGetHeight)(void) = 0;
static int (*pFbGetModeCount)(void) = 0;
static int (*pFbGetModeInfo)(int index, int *width, int *height, int *bpp) = 0;
static int (*pFbSetResolution)(int width, int height, int bpp) = 0;
static uint8_t (*pFbGetPixel)(int x, int y) = 0;
static void (*pFbCapture)(uint8_t *dst, int dst_stride) = 0;
static void (*pFbBlitIndexed)(int x, int y, int w, int h, const uint8_t *src, int src_stride) = 0;

static void (*pWin32kInit)(void *mb_info) = 0;
static HANDLE (*pWin32kRegisterClass)(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t)) = 0;
static HANDLE (*pWin32kCreateWindow)(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) = 0;
static HANDLE (*pWin32kCreateWindowByClass)(HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style) = 0;
static void (*pWin32kShowWindow)(HANDLE hwnd) = 0;
static void (*pWin32kUpdateWindow)(HANDLE hwnd) = 0;
static void (*pWin32kGetClientRect)(HANDLE hwnd, RECT *rect) = 0;
static void (*pWin32kGetClientScreenRect)(HANDLE hwnd, RECT *rect) = 0;
static void (*pWin32kGetWindowRect)(HANDLE hwnd, RECT *rect) = 0;
static void (*pWin32kDestroyWindow)(HANDLE hwnd) = 0;
static void (*pWin32kHandleMouseDown)(int x, int y, int button) = 0;
static void (*pWin32kHandleMouseUp)(int x, int y, int button) = 0;
static void (*pWin32kHandleMouseMove)(int x, int y) = 0;
static void (*pWin32kRedrawAll)(void) = 0;
static void (*pWin32kSetColorPreview)(int enabled) = 0;
static void (*pWin32kRefreshCursor)(void) = 0;
static int (*pWin32kIsDragging)(void) = 0;
static int (*pWin32kIsResizing)(void) = 0;
static HANDLE (*pWin32kGetActiveWindow)(void) = 0;
static void (*pWin32kActivateWindow)(HANDLE hwnd) = 0;
static void (*pWin32kSetWindowIcons)(HANDLE hwnd, HANDLE big_icon, HANDLE small_icon) = 0;

int fb_width = 640;
int fb_height = 480;

void SerialSetDebugEnabled(int enabled) {
    serial_debug_enabled = enabled ? 1 : 0;
    BootSerialSetDebugEnabled(serial_debug_enabled);
}

int SerialIsDebugEnabled(void) { return serial_debug_enabled; }

void SerialSetScreenDebugEnabled(int enabled) {
    int i;
    screen_debug_enabled = enabled ? 1 : 0;
    screen_debug_x = screen_debug_y = 0;
    if (screen_debug_enabled)
        for (i = 0; i < SCREEN_DEBUG_WIDTH * SCREEN_DEBUG_HEIGHT; i++)
            SCREEN_DEBUG_MEMORY[i] = 0x0720;
}

int SerialIsScreenDebugEnabled(void) { return screen_debug_enabled; }

void SerialInit(void) {
    if (!serial_debug_enabled) return;
    if (pSerialInit) pSerialInit(); else BootSerialInit();
}
void SerialPutChar(char c) {
    if (serial_debug_enabled) {
        if (pSerialPutChar) pSerialPutChar(c); else BootSerialPutChar(c);
    }
    if (screen_debug_enabled) ScreenDebugPutChar(c);
}
void SerialPutString(const char *str) {
    if (screen_debug_enabled) {
        while (*str) SerialPutChar(*str++);
    } else if (serial_debug_enabled) {
        if (pSerialPutString) pSerialPutString(str); else BootSerialPutString(str);
    }
}
void SerialPrintHex(uint32_t val) {
    char buf[11];
    int i;
    if (screen_debug_enabled) {
        buf[0] = '0'; buf[1] = 'x';
        for (i = 0; i < 8; i++) buf[i + 2] = "0123456789ABCDEF"[(val >> (28 - i * 4)) & 0xF];
        buf[10] = 0;
        SerialPutString(buf);
    } else if (serial_debug_enabled) {
        if (pSerialPrintHex) pSerialPrintHex(val); else BootSerialPrintHex(val);
    }
}
void SerialPrintDec(uint32_t val) {
    char buf[11];
    int pos = 10;
    if (screen_debug_enabled) {
        buf[pos] = 0;
        do { buf[--pos] = '0' + (val % 10); val /= 10; } while (val);
        SerialPutString(buf + pos);
    } else if (serial_debug_enabled) {
        if (pSerialPrintDec) pSerialPrintDec(val); else BootSerialPrintDec(val);
    }
}

void VgaInit(void) { if (pVgaInit) pVgaInit(); }
void VgaClearScreen(uint8_t color) { if (pVgaClearScreen) pVgaClearScreen(color); }
void VgaPutPixel(int x, int y, uint8_t color) { if (pVgaPutPixel) pVgaPutPixel(x, y, color); }
void VgaFillRect(int x, int y, int w, int h, uint8_t color) { if (pVgaFillRect) pVgaFillRect(x, y, w, h, color); }
void VgaDrawRect(int x, int y, int w, int h, uint8_t color) { if (pVgaDrawRect) pVgaDrawRect(x, y, w, h, color); }
void VgaDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg) { if (pVgaDrawChar) pVgaDrawChar(x, y, c, fg, bg); }
void VgaDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg) { if (pVgaDrawString) pVgaDrawString(x, y, str, fg, bg); }
void VgaSwapBuffers(void) { if (pVgaSwapBuffers) pVgaSwapBuffers(); }

void CdfsInit(void) { if (pCdfsInit) pCdfsInit(); else BootCdfsInit(); }
int CdfsReadSector(uint32_t lba, uint8_t *buffer) { return pCdfsReadSector ? pCdfsReadSector(lba, buffer) : BootCdfsReadSector(lba, buffer); }
int CdfsFindFile(const char *path, uint32_t *out_lba, uint32_t *out_size) { return pCdfsFindFile ? pCdfsFindFile(path, out_lba, out_size) : BootCdfsFindFile(path, out_lba, out_size); }
int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size) { return pCdfsReadFile ? pCdfsReadFile(path, out_buffer, out_size) : BootCdfsReadFile(path, out_buffer, out_size); }

void KeyboardInit(void) { if (pKeyboardInit) pKeyboardInit(); }
void KeyboardHandleData(uint8_t data) { if (pKeyboardHandleData) pKeyboardHandleData(data); }
int KeyboardHandleControllerEvent(void) { return pKeyboardHandleControllerEvent ? pKeyboardHandleControllerEvent() : 0; }
int KeyboardPollEvent(KEYBOARD_EVENT *event) { return pKeyboardPollEvent ? pKeyboardPollEvent(event) : 0; }

void MouseInit(void) { if (pMouseInit) pMouseInit(); }
void MouseGetState(MOUSE_STATE *state) { if (pMouseGetState) pMouseGetState(state); }
void MouseDrawCursor(void) { if (pMouseDrawCursor) pMouseDrawCursor(); }
void MouseEraseCursor(void) { if (pMouseEraseCursor) pMouseEraseCursor(); }
void MouseSetCursorType(MOUSE_CURSOR_TYPE type) { if (pMouseSetCursorType) pMouseSetCursorType(type); }
void MouseHandleByte(uint8_t data) { if (pMouseHandleByte) pMouseHandleByte(data); }
void MouseHandleInterrupt(void) { if (pMouseHandleInterrupt) pMouseHandleInterrupt(); }

void NetInit(void) { if (pNetInit) pNetInit(); }
void NetPoll(void) { if (pNetPoll) pNetPoll(); }
int NetIsReady(void) { return pNetIsReady ? pNetIsReady() : 0; }
int NetPing(const char *ip_text, char *out_text, int out_text_len) { return pNetPing ? pNetPing(ip_text, out_text, out_text_len) : 0; }
int NetResolve(const char *name, char *out_ip, int out_ip_len) { return pNetResolve ? pNetResolve(name, out_ip, out_ip_len) : -1; }

void FbInit(void *multiboot_info) { if (pFbInit) pFbInit(multiboot_info); if (pFbGetWidth) fb_width = pFbGetWidth(); if (pFbGetHeight) fb_height = pFbGetHeight(); }
void FbClearScreen(uint8_t color) { if (pFbClearScreen) pFbClearScreen(color); }
void FbPutPixel(int x, int y, uint8_t color) { if (pFbPutPixel) pFbPutPixel(x, y, color); }
void FbFillRect(int x, int y, int w, int h, uint8_t color) { if (pFbFillRect) pFbFillRect(x, y, w, h, color); }
void FbFillRectRGB(int x, int y, int w, int h, uint32_t rgb) { if (pFbFillRectRGB) pFbFillRectRGB(x, y, w, h, rgb); }
void FbDrawRect(int x, int y, int w, int h, uint8_t color) { if (pFbDrawRect) pFbDrawRect(x, y, w, h, color); }
void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg) { if (pFbDrawChar) pFbDrawChar(x, y, c, fg, bg); }
void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg) { if (pFbDrawString) pFbDrawString(x, y, str, fg, bg); }
void FbDrawCharTransparent(int x, int y, char c, uint8_t fg) { if (pFbDrawCharTransparent) pFbDrawCharTransparent(x, y, c, fg); }
void FbDrawStringTransparent(int x, int y, const char *str, uint8_t fg) { if (pFbDrawStringTransparent) pFbDrawStringTransparent(x, y, str, fg); }
void FbSwapBuffers(void) { if (pFbSwapBuffers) pFbSwapBuffers(); }
int FbIsFramebuffer(void) { return pFbIsFramebuffer ? pFbIsFramebuffer() : 0; }
int FbGetWidth(void) { return pFbGetWidth ? pFbGetWidth() : fb_width; }
int FbGetHeight(void) { return pFbGetHeight ? pFbGetHeight() : fb_height; }
int FbGetModeCount(void) { return pFbGetModeCount ? pFbGetModeCount() : 0; }
int FbGetModeInfo(int index, int *width, int *height, int *bpp) { return pFbGetModeInfo ? pFbGetModeInfo(index, width, height, bpp) : 0; }
int FbSetResolution(int width, int height, int bpp) {
    int ok = pFbSetResolution ? pFbSetResolution(width, height, bpp) : 0;
    if (ok && pFbGetWidth) fb_width = pFbGetWidth();
    if (ok && pFbGetHeight) fb_height = pFbGetHeight();
    return ok;
}
uint8_t FbGetPixel(int x, int y) { return pFbGetPixel ? pFbGetPixel(x, y) : 0; }
uint32_t FbGetPixelRGB(int x, int y) { return pFbGetPixelRGB ? pFbGetPixelRGB(x, y) : 0; }
void FbPutPixelRGB(int x, int y, uint32_t rgb) { if (pFbPutPixelRGB) pFbPutPixelRGB(x, y, rgb); }
int FbPaintWallpaper(int x, int y, int w, int h, const char *path) { return pFbPaintWallpaper ? pFbPaintWallpaper(x, y, w, h, path) : 0; }
void FbCaptureRGB(int x, int y, int w, int h, uint32_t *dst, int dst_stride) { if (pFbCaptureRGB) pFbCaptureRGB(x, y, w, h, dst, dst_stride); }
void FbBlitRGB(int x, int y, int w, int h, const uint32_t *src, int src_stride) { if (pFbBlitRGB) pFbBlitRGB(x, y, w, h, src, src_stride); }
void FbSetClipRect(int x, int y, int w, int h) { if (pFbSetClipRect) pFbSetClipRect(x, y, w, h); }
void FbResetClipRect(void) { if (pFbResetClipRect) pFbResetClipRect(); }
void FbCapture(uint8_t *dst, int dst_stride) { if (pFbCapture) pFbCapture(dst, dst_stride); }
void FbBlitIndexed(int x, int y, int w, int h, const uint8_t *src, int src_stride) { if (pFbBlitIndexed) pFbBlitIndexed(x, y, w, h, src, src_stride); }

void Win32kInit(void *mb_info) {
    if (pWin32kInit) pWin32kInit(mb_info);
    else SerialPutString("[STUB] Win32kInit unresolved\r\n");
}
HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t)) {
    if (pWin32kRegisterClass) return pWin32kRegisterClass(className, style, wndProc);
    SerialPutString("[STUB] Win32kRegisterClass unresolved\r\n");
    return INVALID_HANDLE;
}
HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) {
    if (pWin32kCreateWindow) return pWin32kCreateWindow(className, title, x, y, w, h, style);
    SerialPutString("[STUB] Win32kCreateWindow unresolved\r\n");
    return INVALID_HANDLE;
}
HANDLE Win32kCreateWindowByClass(HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style) {
    if (pWin32kCreateWindowByClass) return pWin32kCreateWindowByClass(hClass, title, x, y, w, h, style);
    SerialPutString("[STUB] Win32kCreateWindowByClass unresolved\r\n");
    return INVALID_HANDLE;
}
void Win32kShowWindow(HANDLE hwnd) { if (pWin32kShowWindow) pWin32kShowWindow(hwnd); }
void Win32kUpdateWindow(HANDLE hwnd) { if (pWin32kUpdateWindow) pWin32kUpdateWindow(hwnd); }
void Win32kGetClientRect(HANDLE hwnd, RECT *rect) { if (pWin32kGetClientRect) pWin32kGetClientRect(hwnd, rect); }
void Win32kGetClientScreenRect(HANDLE hwnd, RECT *rect) { if (pWin32kGetClientScreenRect) pWin32kGetClientScreenRect(hwnd, rect); }
void Win32kGetWindowRect(HANDLE hwnd, RECT *rect) { if (pWin32kGetWindowRect) pWin32kGetWindowRect(hwnd, rect); }
void Win32kDestroyWindow(HANDLE hwnd) { if (pWin32kDestroyWindow) pWin32kDestroyWindow(hwnd); }
void Win32kHandleMouseDown(int x, int y, int button) { if (pWin32kHandleMouseDown) pWin32kHandleMouseDown(x, y, button); }
void Win32kHandleMouseUp(int x, int y, int button) { if (pWin32kHandleMouseUp) pWin32kHandleMouseUp(x, y, button); }
void Win32kHandleMouseMove(int x, int y) { if (pWin32kHandleMouseMove) pWin32kHandleMouseMove(x, y); }
void Win32kRedrawAll(void) { if (pWin32kRedrawAll) pWin32kRedrawAll(); }
void Win32kSetColorPreview(int enabled) { if (pWin32kSetColorPreview) pWin32kSetColorPreview(enabled); }
void Win32kRefreshCursor(void) { if (pWin32kRefreshCursor) pWin32kRefreshCursor(); }
int Win32kIsDragging(void) { return pWin32kIsDragging ? pWin32kIsDragging() : 0; }
int Win32kIsResizing(void) { return pWin32kIsResizing ? pWin32kIsResizing() : 0; }
HANDLE Win32kGetActiveWindow(void) { return pWin32kGetActiveWindow ? pWin32kGetActiveWindow() : INVALID_HANDLE; }
void Win32kActivateWindow(HANDLE hwnd) { if (pWin32kActivateWindow) pWin32kActivateWindow(hwnd); }
void Win32kSetWindowIcons(HANDLE hwnd, HANDLE big_icon, HANDLE small_icon) {
    if (pWin32kSetWindowIcons) pWin32kSetWindowIcons(hwnd, big_icon, small_icon);
}

#define RESOLVE(dst, image, name) dst = (void*)PeGetProcAddress(image, name)

void DriverInstallSerial(void *image) {
    RESOLVE(pSerialInit, image, "SerialInit");
    RESOLVE(pSerialPutChar, image, "SerialPutChar");
    RESOLVE(pSerialPutString, image, "SerialPutString");
    RESOLVE(pSerialPrintHex, image, "SerialPrintHex");
    RESOLVE(pSerialPrintDec, image, "SerialPrintDec");

    if (pSerialInit) {
        pSerialInit();
    }
}

void DriverInstallVga(void *image) {
    RESOLVE(pVgaInit, image, "VgaInit");
    RESOLVE(pVgaClearScreen, image, "VgaClearScreen");
    RESOLVE(pVgaPutPixel, image, "VgaPutPixel");
    RESOLVE(pVgaFillRect, image, "VgaFillRect");
    RESOLVE(pVgaDrawRect, image, "VgaDrawRect");
    RESOLVE(pVgaDrawChar, image, "VgaDrawChar");
    RESOLVE(pVgaDrawString, image, "VgaDrawString");
    RESOLVE(pVgaSwapBuffers, image, "VgaSwapBuffers");
}

void DriverInstallCdfs(void *image) {
    RESOLVE(pCdfsInit, image, "CdfsInit");
    RESOLVE(pCdfsReadSector, image, "CdfsReadSector");
    RESOLVE(pCdfsFindFile, image, "CdfsFindFile");
    RESOLVE(pCdfsReadFile, image, "CdfsReadFile");

    if (pCdfsInit && !Fat32IsMounted()) {
        pCdfsInit();
    }
}

void DriverInstallKeyboard(void *image) {
    RESOLVE(pKeyboardInit, image, "KeyboardInit");
    RESOLVE(pKeyboardHandleData, image, "KeyboardHandleData");
    RESOLVE(pKeyboardHandleControllerEvent, image, "KeyboardHandleControllerEvent");
    RESOLVE(pKeyboardPollEvent, image, "KeyboardPollEvent");
}

void DriverInstallMouse(void *image) {
    RESOLVE(pMouseInit, image, "MouseInit");
    RESOLVE(pMouseGetState, image, "MouseGetState");
    RESOLVE(pMouseDrawCursor, image, "MouseDrawCursor");
    RESOLVE(pMouseEraseCursor, image, "MouseEraseCursor");
    RESOLVE(pMouseSetCursorType, image, "MouseSetCursorType");
    RESOLVE(pMouseHandleByte, image, "MouseHandleByte");
    RESOLVE(pMouseHandleInterrupt, image, "MouseHandleInterrupt");
}

void DriverInstallNet(void *image) {
    RESOLVE(pNetInit, image, "NetInit");
    RESOLVE(pNetPoll, image, "NetPoll");
    RESOLVE(pNetIsReady, image, "NetIsReady");
    RESOLVE(pNetPing, image, "NetPing");
    RESOLVE(pNetResolve, image, "NetResolve");
}

void DriverInstallFb(void *image) {
    RESOLVE(pFbInit, image, "FbInit");
    RESOLVE(pFbClearScreen, image, "FbClearScreen");
    RESOLVE(pFbPutPixel, image, "FbPutPixel");
    RESOLVE(pFbFillRect, image, "FbFillRect");
    RESOLVE(pFbFillRectRGB, image, "FbFillRectRGB");
    RESOLVE(pFbDrawRect, image, "FbDrawRect");
    RESOLVE(pFbDrawChar, image, "FbDrawChar");
    RESOLVE(pFbDrawString, image, "FbDrawString");
    RESOLVE(pFbDrawCharTransparent, image, "FbDrawCharTransparent");
    RESOLVE(pFbDrawStringTransparent, image, "FbDrawStringTransparent");
    RESOLVE(pFbGetPixelRGB, image, "FbGetPixelRGB");
    RESOLVE(pFbPutPixelRGB, image, "FbPutPixelRGB");
    RESOLVE(pFbPaintWallpaper, image, "FbPaintWallpaper");
    RESOLVE(pFbCaptureRGB, image, "FbCaptureRGB");
    RESOLVE(pFbBlitRGB, image, "FbBlitRGB");
    RESOLVE(pFbSetClipRect, image, "FbSetClipRect");
    RESOLVE(pFbResetClipRect, image, "FbResetClipRect");
    RESOLVE(pFbSwapBuffers, image, "FbSwapBuffers");
    RESOLVE(pFbIsFramebuffer, image, "FbIsFramebuffer");
    RESOLVE(pFbGetWidth, image, "FbGetWidth");
    RESOLVE(pFbGetHeight, image, "FbGetHeight");
    RESOLVE(pFbGetModeCount, image, "FbGetModeCount");
    RESOLVE(pFbGetModeInfo, image, "FbGetModeInfo");
    RESOLVE(pFbSetResolution, image, "FbSetResolution");
    RESOLVE(pFbGetPixel, image, "FbGetPixel");
    RESOLVE(pFbCapture, image, "FbCapture");
    RESOLVE(pFbBlitIndexed, image, "FbBlitIndexed");
}

void DriverInstallWin32k(void *image) {
    RESOLVE(pWin32kInit, image, "Win32kInit");
    RESOLVE(pWin32kRegisterClass, image, "Win32kRegisterClass");
    RESOLVE(pWin32kCreateWindow, image, "Win32kCreateWindow");
    RESOLVE(pWin32kCreateWindowByClass, image, "Win32kCreateWindowByClass");
    RESOLVE(pWin32kShowWindow, image, "Win32kShowWindow");
    RESOLVE(pWin32kUpdateWindow, image, "Win32kUpdateWindow");
    RESOLVE(pWin32kGetClientRect, image, "Win32kGetClientRect");
    RESOLVE(pWin32kGetClientScreenRect, image, "Win32kGetClientScreenRect");
    RESOLVE(pWin32kGetWindowRect, image, "Win32kGetWindowRect");
    RESOLVE(pWin32kDestroyWindow, image, "Win32kDestroyWindow");
    RESOLVE(pWin32kHandleMouseDown, image, "Win32kHandleMouseDown");
    RESOLVE(pWin32kHandleMouseUp, image, "Win32kHandleMouseUp");
    RESOLVE(pWin32kHandleMouseMove, image, "Win32kHandleMouseMove");
    RESOLVE(pWin32kRedrawAll, image, "Win32kRedrawAll");
    RESOLVE(pWin32kSetColorPreview, image, "Win32kSetColorPreview");
    RESOLVE(pWin32kRefreshCursor, image, "Win32kRefreshCursor");
    RESOLVE(pWin32kIsDragging, image, "Win32kIsDragging");
    RESOLVE(pWin32kIsResizing, image, "Win32kIsResizing");
    RESOLVE(pWin32kGetActiveWindow, image, "Win32kGetActiveWindow");
    RESOLVE(pWin32kActivateWindow, image, "Win32kActivateWindow");
    RESOLVE(pWin32kSetWindowIcons, image, "Win32kSetWindowIcons");

    if (!pWin32kInit) SerialPutString("[STUB] Missing Win32kInit export\r\n");
    if (!pWin32kRegisterClass) SerialPutString("[STUB] Missing Win32kRegisterClass export\r\n");
    if (!pWin32kCreateWindow) SerialPutString("[STUB] Missing Win32kCreateWindow export\r\n");
    if (!pWin32kCreateWindowByClass) SerialPutString("[STUB] Missing Win32kCreateWindowByClass export\r\n");
}
