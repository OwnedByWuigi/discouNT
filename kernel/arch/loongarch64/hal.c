#include <stdint.h>
#include "hal.h"
#include "serial.h"

#define WIDTH 640
#define HEIGHT 480
#define COLS 80
#define ROWS 60
#define PCI_DEVICE2 0x20010000UL
#define PCI_MEM_BASE 0UL
#define FB_BUS 0x40000000U
#define MMIO_BUS 0x41000000U
#define FB ((volatile uint32_t *)(PCI_MEM_BASE + FB_BUS))
#define VBE ((volatile uint16_t *)(PCI_MEM_BASE + MMIO_BUS + 0x500))

static int cursor_x, cursor_y, display_ready;
static uint8_t attribute = 0x1f;
static const uint8_t font[][7] = {
 {0,0,0,0,0,0,0},{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

static uint32_t color(uint8_t n) {
 static const uint32_t c[16]={0,0xaa,0xaa00,0xaaaa,0xaa0000,0xaa00aa,0xaa5500,0xaaaaaa,0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff};
 return c[n&15];
}
static const uint8_t *glyph(char c) {
 if(c>='a'&&c<='z')c-=32; if(c==' ')return font[0];
 if(c>='0'&&c<='9')return font[1+c-'0']; if(c>='A'&&c<='Z')return font[11+c-'A']; return font[0];
}
static void cell(int x,int y,char c,uint8_t a) {
 const uint8_t *g=glyph(c); int px,py;
 if(!display_ready||x<0||x>=COLS||y<0||y>=ROWS)return;
 for(py=0;py<8;py++)for(px=0;px<8;px++)FB[(y*8+py)*WIDTH+x*8+px]=color(py<7&&px<5&&(g[py]&(1U<<(4-px)))?a:a>>4);
}
static void scroll(void) {
 uint32_t x,y; for(y=8;y<HEIGHT;y++)for(x=0;x<WIDTH;x++)FB[(y-8)*WIDTH+x]=FB[y*WIDTH+x];
 for(y=HEIGHT-8;y<HEIGHT;y++)for(x=0;x<WIDTH;x++)FB[y*WIDTH+x]=color(attribute>>4); cursor_y=ROWS-1;
}
static int display_init(void) {
 volatile uint32_t *cfg=(volatile uint32_t*)PCI_DEVICE2;
 if((cfg[0]&0xffff)==0xffff)return 0;
 cfg[4]=FB_BUS; cfg[6]=MMIO_BUS; cfg[1]=(cfg[1]&0xffff0000U)|2;
 VBE[4]=0; VBE[1]=WIDTH; VBE[2]=HEIGHT; VBE[3]=32; VBE[4]=0x41; return 1;
}
void HalInitialize(void){SerialInit();display_ready=display_init();SerialPutString(display_ready?"[HAL] Bochs framebuffer initialized\r\n":"[HAL] No Bochs framebuffer found\r\n");}
void HalConfigureBootDisplay(void *p){(void)p;}
void HalClearScreen(uint8_t a){uint32_t i;cursor_x=cursor_y=0;attribute=a;if(display_ready)for(i=0;i<WIDTH*HEIGHT;i++)FB[i]=color(a>>4);}
void HalPutChar(char c,uint8_t a){attribute=a;SerialPutChar(c);if(c=='\n'){cursor_x=0;cursor_y++;}else if(c=='\r')cursor_x=0;else{cell(cursor_x,cursor_y,c,a);if(++cursor_x>=COLS){cursor_x=0;cursor_y++;}}if(cursor_y>=ROWS&&display_ready)scroll();}
void HalPutString(const char*s,uint8_t a){while(s&&*s)HalPutChar(*s++,a);}
void HalSetCursor(int x,int y){cursor_x=x;cursor_y=y;} void HalGetCursor(int*x,int*y){if(x)*x=cursor_x;if(y)*y=cursor_y;} uint8_t HalGetAttribute(void){return attribute;}
