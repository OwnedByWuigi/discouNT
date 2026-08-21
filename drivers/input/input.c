#include <stdint.h>
#include "input.h"
#include "io/port.h"
int InputReadControllerByte(int *mouse,uint8_t *data){
#if defined(__loongarch64)
 (void)mouse;(void)data;return 0;
#else
 uint8_t status=inb(0x64);if(!(status&1))return 0;if(mouse)*mouse=(status&0x20)!=0;if(data)*data=inb(0x60);return 1;
#endif
}
