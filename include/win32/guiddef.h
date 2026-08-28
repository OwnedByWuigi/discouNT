#ifndef DISCOUNT_GUIDDEF_H
#define DISCOUNT_GUIDDEF_H
#include "windef.h"
typedef struct _GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID;
typedef GUID IID, CLSID;
typedef const GUID *REFGUID;
typedef GUID *LPGUID;
typedef const IID *REFIID;
typedef const CLSID *REFCLSID;
static inline BOOL IsEqualGUID(REFGUID a, REFGUID b) {
    const DWORD *x=(const DWORD*)a,*y=(const DWORD*)b;
    return x && y && x[0]==y[0] && x[1]==y[1] && x[2]==y[2] && x[3]==y[3];
}
#define IsEqualIID IsEqualGUID
#define IsEqualCLSID IsEqualGUID
#define DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
    const GUID name={l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#endif
