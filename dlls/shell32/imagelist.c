#include "commoncontrols.h"
#include "shellapi.h"

const IID IID_IImageList={0x46eb5926,0x582e,0x4017,{0x9f,0xdf,0xe8,0x99,0x8d,0xaa,0x09,0x50}};
typedef struct { IImageList iface; ULONG refs; } SYSTEM_IMAGE_LIST;
static HRESULT WINAPI image_qi(IImageList*i,REFIID id,void**out){SYSTEM_IMAGE_LIST*l=(SYSTEM_IMAGE_LIST*)i;if(!out)return E_POINTER;*out=0;if(!IsEqualIID(id,&IID_IUnknown)&&!IsEqualIID(id,&IID_IImageList))return E_NOINTERFACE;*out=i;l->refs++;return S_OK;}
static ULONG WINAPI image_add(IImageList*i){return ++((SYSTEM_IMAGE_LIST*)i)->refs;}
static ULONG WINAPI image_release(IImageList*i){SYSTEM_IMAGE_LIST*l=(SYSTEM_IMAGE_LIST*)i;if(l->refs>1)l->refs--;return l->refs;}
static const IImageListVtbl image_vtbl={image_qi,image_add,image_release};
static SYSTEM_IMAGE_LIST small_images={{&image_vtbl},1},large_images={{&image_vtbl},1};

HRESULT WINAPI SHGetImageList(int which,REFIID iid,void **out){SYSTEM_IMAGE_LIST*l=which==SHIL_LARGE?&large_images:&small_images;return image_qi(&l->iface,iid,out);}
DWORD_PTR WINAPI SHGetFileInfoW(LPCWSTR path,DWORD attributes,SHFILEINFOW *info,UINT size,UINT flags){SYSTEM_IMAGE_LIST*l=(flags&SHGFI_SMALLICON)?&small_images:&large_images;(void)path;(void)attributes;if(info&&size>=sizeof(*info)){info->iIcon=0;info->hIcon=0;info->dwAttributes=0;info->szDisplayName[0]=0;info->szTypeName[0]=0;}if(flags&SHGFI_SYSICONINDEX){image_add(&l->iface);return (DWORD_PTR)&l->iface;}return 1;}
