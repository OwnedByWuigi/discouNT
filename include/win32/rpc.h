#ifndef DISCOUNT_RPC_H
#define DISCOUNT_RPC_H
#include "windows.h"
typedef LONG RPC_STATUS;
typedef GUID UUID;
#define RPC_S_OK 0
#define RPC_S_UUID_LOCAL_ONLY 1824
RPC_STATUS WINAPI UuidCreate(UUID *uuid);
#endif
