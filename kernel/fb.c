#include <stdint.h>
#include "fb.h"
#include "vga.h"
#include "portio.h"
#include "serial.h"

int fb_width = 640;
int fb_height = 480;

static int use_framebuffer = 0;
static uint16_t *fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

static const uint16_t vga_to_rgb565[16] = {
    0x0000, 0x001F, 0x07E0, 0x07FF, 0xF800, 0xF81F, 0xA145, 0xC618,
    0x4208, 0x3C7F, 0x87E0, 0x87FF, 0xFC10, 0xFC1F, 0xFFE0, 0xFFFF
};

// VBE PMInfo block
typedef struct {
    uint16_t setWindow;
    uint16_t setDisplayStart;
    uint16_t setPalette;
    uint16_t IOBase;
} __attribute__((packed)) VBEPMInfo;

void FbInit(void *mb_info_ptr) {
    uint32_t *mb = (uint32_t*)mb_info_ptr;
    uint32_t flags = mb[0];
    
    SerialPutString("[FB] Multiboot flags: 0x");
    SerialPrintHex(flags);
    SerialPutString("\r\n");
    
    // Check for VBE info (bit 11)
    if (flags & (1 << 11)) {
        uint32_t vbe_ctrl_info = mb[11];
        uint32_t vbe_mode_info = mb[12];
        uint32_t vbe_interface_seg = mb[14];
        uint32_t vbe_interface_off = mb[15];
        uint32_t vbe_interface_len = mb[16];
        
        SerialPutString("[VBE] Control info: 0x");
        SerialPrintHex(vbe_ctrl_info);
        SerialPutString("\r\n");
        SerialPutString("[VBE] Interface: 0x");
        SerialPrintHex(vbe_interface_seg);
        SerialPutString(":0x");
        SerialPrintHex(vbe_interface_off);
        SerialPutString(" (len=");
        SerialPrintDec(vbe_interface_len);
        SerialPutString(")\r\n");
        
        // Get the VBE protected mode interface
        if (vbe_interface_seg != 0 && vbe_interface_off != 0) {
            uint32_t pm_interface = (vbe_interface_seg * 16) + vbe_interface_off;
            VBEPMInfo *pm = (VBEPMInfo*)(uintptr_t)pm_interface;
            
            SerialPutString("[VBE] PM setWindow: 0x");
            SerialPrintHex(pm->setWindow);
            SerialPutString("\r\n");
            SerialPutString("[VBE] PM setDisplayStart: 0x");
            SerialPrintHex(pm->setDisplayStart);
            SerialPutString("\r\n");
            SerialPutString("[VBE] PM setPalette: 0x");
            SerialPrintHex(pm->setPalette);
            SerialPutString("\r\n");
            SerialPutString("[VBE] PM IOBase: 0x");
            SerialPrintHex(pm->IOBase);
            SerialPutString("\r\n");
            
            // Try to set mode 0x112 (640x480x32) or 0x115 (800x600x16)
            // We need to call the VBE PM interface to set a mode
            
            // The VBE control info at vbe_ctrl_info has the video mode list
            uint8_t *ctrl = (uint8_t*)(uintptr_t)vbe_ctrl_info;
            
            // Get video mode list pointer
            uint32_t video_modes_ptr = *(uint32_t*)(ctrl + 14);
            
            SerialPutString("[VBE] Video modes at: 0x");
            SerialPrintHex(video_modes_ptr);
            SerialPutString("\r\n");
            
            if (video_modes_ptr != 0 && video_modes_ptr < 0x100000) {
                uint16_t *modes = (uint16_t*)(uintptr_t)video_modes_ptr;
                
                // Scan for good modes
                SerialPutString("[VBE] Scanning modes...\r\n");
                for (int i = 0; modes[i] != 0xFFFF && i < 50; i++) {
                    uint16_t mode = modes[i];
                    SerialPutString("[VBE] Mode: 0x");
                    SerialPrintHex(mode);
                    SerialPutString("\r\n");
                    
                    // Try to get mode info for this mode
                    // We need to call the VBE PM interface function
                    // But that requires v8086 mode or a thunk...
                    // This is getting too complex
                }
            }
        }
    }
    
    // Check if framebuffer is directly available (bit 12)
    if (flags & (1 << 12)) {
        uint8_t *bytes = (uint8_t*)mb_info_ptr;
        uint32_t fb_addr_low = *(uint32_t*)(bytes + 88);
        uint32_t fb_pitch_val = *(uint32_t*)(bytes + 96);
        uint32_t fb_width_val = *(uint32_t*)(bytes + 100);
        uint32_t fb_height_val = *(uint32_t*)(bytes + 104);
        uint8_t fb_bpp_val = bytes[108];
        
        SerialPutString("[FB] addr=0x");
        SerialPrintHex(fb_addr_low);
        SerialPutString(" w=");
        SerialPrintDec(fb_width_val);
        SerialPutString(" h=");
        SerialPrintDec(fb_height_val);
        SerialPutString(" bpp=");
        SerialPrintDec(fb_bpp_val);
        SerialPutString("\r\n");
        
        if (fb_addr_low > 0x100000 && fb_width_val >= 640 && fb_bpp_val >= 16) {
            fb_addr = (uint16_t*)(uintptr_t)fb_addr_low;
            fb_pitch = fb_pitch_val;
            fb_width = fb_width_val;
            fb_height = fb_height_val;
            fb_bpp = fb_bpp_val;
            use_framebuffer = 1;
            SerialPutString("[FB] Multiboot framebuffer active!\r\n");
            return;
        }
    }
    
    // The ONLY reliable way: use the multiboot header to force graphics
    // If we got here, GRUB didn't give us graphics mode
    // We need to fix the multiboot header and GRUB config
    
    SerialPutString("[FB] No graphics framebuffer from GRUB\r\n");
    SerialPutString("[FB] Make sure GRUB config has:\r\n");
    SerialPutString("[FB]   set gfxpayload=800x600x16\r\n");
    SerialPutString("[FB] And boot.asm has mode_type=1\r\n");
    SerialPutString("[FB] Falling back to VGA\r\n");
    
    VgaInit();
    fb_width = 640;
    fb_height = 480;
    use_framebuffer = 0;
}

int FbIsFramebuffer(void) { return use_framebuffer; }
int FbGetWidth(void) { return fb_width; }
int FbGetHeight(void) { return fb_height; }

void FbClearScreen(uint8_t color) {
    if (use_framebuffer) {
        uint16_t rgb = vga_to_rgb565[color & 0x0F];
        uint32_t pp = fb_pitch / 2;
        for (int y = 0; y < fb_height; y++)
            for (int x = 0; x < fb_width; x++)
                fb_addr[y * pp + x] = rgb;
    } else {
        VgaClearScreen(color);
    }
}

void FbPutPixel(int x, int y, uint8_t color) {
    if (use_framebuffer) {
        if (x >= 0 && x < fb_width && y >= 0 && y < fb_height) {
            uint32_t pp = fb_pitch / 2;
            fb_addr[y * pp + x] = vga_to_rgb565[color & 0x0F];
        }
    } else {
        VgaPutPixel(x, y, color);
    }
}

void FbFillRect(int x, int y, int w, int h, uint8_t color) {
    if (use_framebuffer) {
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > fb_width) w = fb_width - x;
        if (y + h > fb_height) h = fb_height - y;
        if (w <= 0 || h <= 0) return;
        uint16_t rgb = vga_to_rgb565[color & 0x0F];
        uint32_t pp = fb_pitch / 2;
        for (int row = y; row < y + h; row++)
            for (int col = x; col < x + w; col++)
                fb_addr[row * pp + col] = rgb;
    } else {
        VgaFillRect(x, y, w, h, color);
    }
}

void FbDrawRect(int x, int y, int w, int h, uint8_t color) {
    FbFillRect(x, y, w, 1, color);
    FbFillRect(x, y + h - 1, w, 1, color);
    FbFillRect(x, y, 1, h, color);
    FbFillRect(x + w - 1, y, 1, h, color);
}

void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg) {
    VgaDrawChar(x, y, c, fg, bg);
}

void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg) {
    VgaDrawString(x, y, str, fg, bg);
}

void FbSwapBuffers(void) {
    if (use_framebuffer) {
        uint32_t pp = fb_pitch / 2;
        int max_y = fb_height < 480 ? fb_height : 480;
        int max_x = fb_width < 640 ? fb_width : 640;
        for (int y = 0; y < max_y; y++)
            for (int x = 0; x < max_x; x++)
                fb_addr[y * pp + x] = vga_to_rgb565[back_buffer[y * 640 + x] & 0x0F];
    } else {
        VgaSwapBuffers();
    }
}