#include <stdint.h>
#include "input.h"
#include "io/port.h"

#define INPUT_QUEUE_SIZE 64
typedef struct { uint8_t mouse, data; } INPUT_BYTE;
static INPUT_BYTE input_queue[INPUT_QUEUE_SIZE];
static uint8_t input_head, input_tail;

static void input_inject(uint8_t mouse, uint8_t data) {
 uint8_t next=(uint8_t)((input_head+1)%INPUT_QUEUE_SIZE);
 if(next==input_tail)return;
 input_queue[input_head].mouse=mouse;input_queue[input_head].data=data;input_head=next;
}
void InputInjectKeyboardByte(uint8_t data){input_inject(0,data);}
void InputInjectMouseByte(uint8_t data){input_inject(1,data);}

int InputReadControllerByte(int *mouse,uint8_t *data){
 if(input_tail!=input_head){
  if(mouse)*mouse=input_queue[input_tail].mouse;
  if(data)*data=input_queue[input_tail].data;
  input_tail=(uint8_t)((input_tail+1)%INPUT_QUEUE_SIZE);return 1;
 }
#if defined(__loongarch64)
 (void)mouse;(void)data;return 0;
#else
 uint8_t status=inb(0x64);if(!(status&1))return 0;if(mouse)*mouse=(status&0x20)!=0;if(data)*data=inb(0x60);return 1;
#endif
}
