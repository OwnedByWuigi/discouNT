#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>

#define COM1_PORT 0x3F8

void SerialInit(void);
void SerialSetDebugEnabled(int enabled);
int SerialIsDebugEnabled(void);
void SerialSetScreenDebugEnabled(int enabled);
int SerialIsScreenDebugEnabled(void);
void SerialPutChar(char c);
void SerialPutString(const char *str);
void SerialPrintHex(uint32_t val);
void SerialPrintDec(uint32_t val);
#endif
