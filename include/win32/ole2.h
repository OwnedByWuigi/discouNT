#ifndef DISCOUNT_OLE2_H
#define DISCOUNT_OLE2_H
#include "objbase.h"
static inline HRESULT WINAPI OleInitialize(LPVOID reserved){return CoInitialize(reserved);}
static inline void WINAPI OleUninitialize(void){CoUninitialize();}
#endif
