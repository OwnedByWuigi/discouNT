#include "rpc.h"

RPC_STATUS WINAPI UuidCreate(UUID *uuid)
{
    static volatile LONG sequence;
    ULONGLONG value;
    if(!uuid)return 87;
    value=((ULONGLONG)GetTickCount()<<32)|(ULONG)InterlockedIncrement(&sequence);
    uuid->Data1=(DWORD)value;
    uuid->Data2=(WORD)(value>>32);
    uuid->Data3=(WORD)((value>>48)&0x0fff)|0x4000;
    uuid->Data4[0]=0x80;uuid->Data4[1]=0;
    for(int i=2;i<8;i++)uuid->Data4[i]=(BYTE)(value>>((i-2)*8));
    return RPC_S_OK;
}
int WINAPI DllMain(void*m,DWORD r,void*p){(void)m;(void)r;(void)p;return 1;}
