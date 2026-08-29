#ifndef DISCOUNT_WDF_WINIOCTL_H
#define DISCOUNT_WDF_WINIOCTL_H

#include <stdint.h>

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((uint32_t)(DeviceType) << 16) | ((uint32_t)(Access) << 14) | \
     ((uint32_t)(Function) << 2) | (uint32_t)(Method))
#endif
#define METHOD_BUFFERED 0
#define METHOD_IN_DIRECT 1
#define METHOD_OUT_DIRECT 2
#define METHOD_NEITHER 3
#define FILE_ANY_ACCESS 0
#define FILE_READ_ACCESS 1
#define FILE_WRITE_ACCESS 2

#endif
