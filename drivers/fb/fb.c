#include <stdint.h>
#include "fb.h"
#include "vga.h"
#include "serial.h"

int fb_width = 640;
int fb_height = 480;

static int use_framebuffer = 0;
static uint8_t *fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;
static uint8_t fb_type = 0;
static uint32_t fb_addr_hi = 0;

static const uint16_t vga_to_rgb565[16] = {
    0x0000, 0x001F, 0x07E0, 0x07FF, 0xF800, 0xF81F, 0xA145, 0xC618,
    0x4208, 0x3C7F, 0x87E0, 0x87FF, 0xFC10, 0xFC1F, 0xFFE0, 0xFFFF
};

static const uint32_t vga_to_rgb888[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

typedef struct {
    uint16_t mode_attributes;
    uint8_t win_a;
    uint8_t win_b;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_seg_a;
    uint16_t win_seg_b;
    uint32_t win_func_ptr;
    uint16_t bytes_per_scanline;
    uint16_t width;
    uint16_t height;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    uint32_t phys_base_ptr;
    uint32_t offscreen_mem_offset;
    uint16_t offscreen_mem_size;
} __attribute__((packed)) VBEModeInfo;

static void fb_set_backbuffer_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= 640 || y < 0 || y >= 480) return;
    back_buffer[y * 640 + x] = color & 0x0F;
}

static void fb_clear_backbuffer(uint8_t color) {
    for (int i = 0; i < 640 * 480; i++) {
        back_buffer[i] = color & 0x0F;
    }
}

static void fb_flush_to_lfb(void) {
    if (!use_framebuffer || !fb_addr) return;
    
    int max_y = fb_height < 480 ? fb_height : 480;
    int max_x = fb_width < 640 ? fb_width : 640;
    
    for (int y = 0; y < max_y; y++) {
        uint8_t *row = fb_addr + (y * fb_pitch);
        for (int x = 0; x < max_x; x++) {
            uint8_t color = back_buffer[y * 640 + x] & 0x0F;
            
            if (fb_bpp == 16) {
                ((uint16_t*)row)[x] = vga_to_rgb565[color];
            } else if (fb_bpp == 24) {
                uint32_t rgb = vga_to_rgb888[color];
                uint8_t *pixel = row + (x * 3);
                pixel[0] = rgb & 0xFF;
                pixel[1] = (rgb >> 8) & 0xFF;
                pixel[2] = (rgb >> 16) & 0xFF;
            } else if (fb_bpp == 32) {
                ((uint32_t*)row)[x] = vga_to_rgb888[color];
            }
        }
    }
}

void FbInit(void *mb_info_ptr) {
    if (!mb_info_ptr) {
        SerialPutString("[FB] No multiboot info pointer\r\n");
        VgaInit();
        fb_width = 640;
        fb_height = 480;
        use_framebuffer = 0;
        fb_clear_backbuffer(0);
        return;
    }

    uint32_t *mb = (uint32_t*)mb_info_ptr;
    uint32_t flags = mb[0];
    
    SerialPutString("[FB] Multiboot flags: 0x");
    SerialPrintHex(flags);
    SerialPutString("\r\n");
    
    // Check for VBE info (bit 11) using the Multiboot v1 info layout.
    // Offsets here are fixed byte offsets, not mb[] indices.
    if (flags & (1 << 11)) {
        uint8_t *bytes = (uint8_t*)mb_info_ptr;
        uint32_t vbe_ctrl_info = *(uint32_t*)(bytes + 72);
        uint32_t vbe_mode_info_ptr = *(uint32_t*)(bytes + 76);
        uint16_t vbe_mode = *(uint16_t*)(bytes + 80);
        uint16_t vbe_interface_seg = *(uint16_t*)(bytes + 82);
        uint16_t vbe_interface_off = *(uint16_t*)(bytes + 84);
        uint16_t vbe_interface_len = *(uint16_t*)(bytes + 86);
        
        SerialPutString("[VBE] Control info: 0x");
        SerialPrintHex(vbe_ctrl_info);
        SerialPutString("\r\n");
        SerialPutString("[VBE] Mode info: 0x");
        SerialPrintHex(vbe_mode_info_ptr);
        SerialPutString(" mode=0x");
        SerialPrintHex(vbe_mode);
        SerialPutString("\r\n");
        SerialPutString("[VBE] Interface: 0x");
        SerialPrintHex(vbe_interface_seg);
        SerialPutString(":0x");
        SerialPrintHex(vbe_interface_off);
        SerialPutString(" (len=");
        SerialPrintDec(vbe_interface_len);
        SerialPutString(")\r\n");

        if (vbe_mode_info_ptr != 0) {
            VBEModeInfo *mode_info = (VBEModeInfo*)(uintptr_t)vbe_mode_info_ptr;

            SerialPutString("[VBE] attrs=0x");
            SerialPrintHex(mode_info->mode_attributes);
            SerialPutString(" phys=0x");
            SerialPrintHex(mode_info->phys_base_ptr);
            SerialPutString(" pitch=");
            SerialPrintDec(mode_info->bytes_per_scanline);
            SerialPutString(" w=");
            SerialPrintDec(mode_info->width);
            SerialPutString(" h=");
            SerialPrintDec(mode_info->height);
            SerialPutString(" bpp=");
            SerialPrintDec(mode_info->bpp);
            SerialPutString(" mem=");
            SerialPrintDec(mode_info->memory_model);
            SerialPutString("\r\n");

            if ((mode_info->mode_attributes & (1 << 4)) &&
                (mode_info->mode_attributes & (1 << 7)) &&
                mode_info->phys_base_ptr >= 0x100000 &&
                mode_info->bytes_per_scanline != 0 &&
                mode_info->width >= 640 &&
                mode_info->height >= 480 &&
                (mode_info->bpp == 16 || mode_info->bpp == 24 || mode_info->bpp == 32)) {
                fb_addr = (uint8_t*)(uintptr_t)mode_info->phys_base_ptr;
                fb_addr_hi = 0;
                fb_pitch = mode_info->bytes_per_scanline;
                fb_width = mode_info->width;
                fb_height = mode_info->height;
                fb_bpp = mode_info->bpp;
                fb_type = 1;
                use_framebuffer = 1;
                SerialPutString("[FB] VBE linear framebuffer active!\r\n");
                fb_clear_backbuffer(0);
                fb_flush_to_lfb();
                return;
            }
        }
    }
    
    // Check if framebuffer is directly available (bit 12)
    if (flags & (1 << 12)) {
        uint8_t *bytes = (uint8_t*)mb_info_ptr;
        uint32_t fb_addr_low = *(uint32_t*)(bytes + 88);
        uint32_t fb_addr_high = *(uint32_t*)(bytes + 92);
        uint32_t fb_pitch_val = *(uint32_t*)(bytes + 96);
        uint32_t fb_width_val = *(uint32_t*)(bytes + 100);
        uint32_t fb_height_val = *(uint32_t*)(bytes + 104);
        uint8_t fb_bpp_val = bytes[108];
        uint8_t fb_type_val = bytes[109];
        
        SerialPutString("[FB] addr=0x");
        SerialPrintHex(fb_addr_low);
        SerialPutString(" hi=0x");
        SerialPrintHex(fb_addr_high);
        SerialPutString(" w=");
        SerialPrintDec(fb_width_val);
        SerialPutString(" h=");
        SerialPrintDec(fb_height_val);
        SerialPutString(" bpp=");
        SerialPrintDec(fb_bpp_val);
        SerialPutString(" type=");
        SerialPrintDec(fb_type_val);
        SerialPutString("\r\n");
        
        if (fb_addr_high == 0 &&
            fb_addr_low >= 0x100000 &&
            fb_width_val >= 640 &&
            fb_height_val >= 480 &&
            fb_pitch_val != 0 &&
            fb_type_val == 1 &&
            (fb_bpp_val == 16 || fb_bpp_val == 24 || fb_bpp_val == 32)) {
            fb_addr = (uint8_t*)(uintptr_t)fb_addr_low;
            fb_addr_hi = fb_addr_high;
            fb_pitch = fb_pitch_val;
            fb_width = fb_width_val;
            fb_height = fb_height_val;
            fb_bpp = fb_bpp_val;
            fb_type = fb_type_val;
            use_framebuffer = 1;
            SerialPutString("[FB] Multiboot framebuffer active!\r\n");
            fb_clear_backbuffer(0);
            fb_flush_to_lfb();
            return;
        }
    }
    
    // The ONLY reliable way: use the multiboot header to force graphics
    // If we got here, GRUB didn't give us graphics mode
    // We need to fix the multiboot header and GRUB config
    
    SerialPutString("[FB] No graphics framebuffer from GRUB\r\n");
    SerialPutString("[FB] Make sure GRUB config has:\r\n");
    SerialPutString("[FB]   terminal_output gfxterm\r\n");
    SerialPutString("[FB]   set gfxpayload=keep\r\n");
    SerialPutString("[FB] And boot.asm has mode_type=1\r\n");
    SerialPutString("[FB] Falling back to VGA\r\n");
    
    VgaInit();
    fb_width = 640;
    fb_height = 480;
    use_framebuffer = 0;
    fb_bpp = 0;
    fb_pitch = 0;
    fb_type = 0;
    fb_addr = 0;
    fb_addr_hi = 0;
    fb_clear_backbuffer(0);
}

int FbIsFramebuffer(void) { return use_framebuffer; }
int FbGetWidth(void) { return fb_width; }
int FbGetHeight(void) { return fb_height; }

void FbClearScreen(uint8_t color) {
    if (use_framebuffer) {
        fb_clear_backbuffer(color);
    } else {
        VgaClearScreen(color);
    }
}

void FbPutPixel(int x, int y, uint8_t color) {
    if (use_framebuffer) {
        fb_set_backbuffer_pixel(x, y, color);
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
        for (int row = y; row < y + h; row++)
            for (int col = x; col < x + w; col++)
                fb_set_backbuffer_pixel(col, row, color);
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
        fb_flush_to_lfb();
    } else {
        VgaSwapBuffers();
    }
}
