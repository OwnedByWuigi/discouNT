#ifndef DISCOUNT_COMMONCONTROLS_H
#define DISCOUNT_COMMONCONTROLS_H
#include "objbase.h"
#include "commctrl.h"

typedef struct IImageList IImageList;
typedef struct IImageListVtbl {
 HRESULT(WINAPI*QueryInterface)(IImageList*,REFIID,void**); ULONG(WINAPI*AddRef)(IImageList*); ULONG(WINAPI*Release)(IImageList*);
} IImageListVtbl;
struct IImageList { const IImageListVtbl *lpVtbl; };
#define IImageList_AddRef(p) ((p)->lpVtbl->AddRef((p)))
#define IImageList_Release(p) ((p)->lpVtbl->Release((p)))
extern const IID IID_IImageList;
#define SHIL_LARGE 0
#define SHIL_SMALL 1
HRESULT WINAPI SHGetImageList(int image_list,REFIID iid,void **object);
#endif
