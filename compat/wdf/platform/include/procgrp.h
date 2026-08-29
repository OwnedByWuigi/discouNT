#ifndef DISCOUNT_WDF_PROCGRP_H
#define DISCOUNT_WDF_PROCGRP_H
#include <ntddk.h>
typedef USHORT PROCESSOR_NUMBER_GROUP;
typedef struct _PROCESSOR_NUMBER { USHORT Group; UCHAR Number; UCHAR Reserved; } PROCESSOR_NUMBER;
#endif
