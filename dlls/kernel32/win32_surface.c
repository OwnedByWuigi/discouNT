#include <stdint.h>
#include <stddef.h>
#include "windows.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void *memcpy(void *, const void *, uint32_t);

/* Small Win32/CRT surface used by the unmodified ReactOS Regedit build.
 * These are deliberately ordinary platform entry points, rather than an
 * application compatibility file: native callers can use them too. */
void *BeginDeferWindowPos(int count) { (void)count; return (void *)1; }
void *DeferWindowPos(void *h, void *w, void *i, int x, int y, int cx, int cy, uint32_t f) { (void)w;(void)i;(void)x;(void)y;(void)cx;(void)cy;(void)f; return h; }
int EndDeferWindowPos(void *h) { return h != 0; }
void *CreatePatternBrush(void *bitmap) { return bitmap ? bitmap : (void *)1; }
void *CreateCaret(void *hwnd, void *bitmap, int cx, int cy) { (void)hwnd;(void)bitmap;(void)cx;(void)cy; return (void *)1; }
int DestroyCaret(void) { return 1; }
void *GetDCEx(void *hwnd, void *region, uint32_t flags) { (void)region;(void)flags; return hwnd ? hwnd : (void *)1; }
void ReleaseCapture(void) { }
void SetCapture(void *hwnd) { (void)hwnd; }
void *GetCapture(void) { return 0; }
short GetAsyncKeyState(int key) { (void)key; return 0; }
void *GetNextDlgTabItem(void *dlg, void *ctrl, int previous) { (void)dlg;(void)ctrl;(void)previous; return 0; }
int InflateRect(void *rect, int cx, int cy) { (void)rect;(void)cx;(void)cy; return 1; }
int PatBlt(void *dc, int x, int y, int cx, int cy, uint32_t rop) { (void)dc;(void)x;(void)y;(void)cx;(void)cy;(void)rop; return 1; }
int ScrollWindow(void *hwnd, int dx, int dy, void *scroll, void *clip) { (void)hwnd;(void)dx;(void)dy;(void)scroll;(void)clip; return 1; }
int SetCaretPos(int x, int y) { (void)x;(void)y; return 1; }
int SetCursorPos(int x, int y) { (void)x;(void)y; return 1; }
int ShowCaret(void *hwnd) { (void)hwnd; return 1; }
int MessageBeep(uint32_t type) { (void)type; return 1; }
int SetProcessDefaultLayout(uint32_t layout) { (void)layout; return 1; }
uint32_t GetOEMCP(void) { return 437; }
uint32_t GetUserDefaultUILanguage(void) { return 1033; }
int GetWindowTextLength(void *hwnd) { (void)hwnd; return 0; }
int WriteConsoleW(void *out, const void *text, uint32_t count, uint32_t *written, void *reserved) { (void)out;(void)text;(void)reserved; if (written) *written=count; return 1; }
uint32_t SearchPathW(const uint16_t *path, const uint16_t *file, const uint16_t *ext, uint32_t cap, uint16_t *out, uint16_t **part) { (void)path;(void)file;(void)ext;(void)cap;(void)out;if(part)*part=0;return 0; }
void *GlobalAlloc(uint32_t flags, size_t bytes) { (void)flags; return kmalloc((uint32_t)bytes); }
void *GlobalFree(void *p) { if (p) kfree(p); return 0; }
void *GlobalLock(void *p) { return p; }
int GlobalUnlock(void *p) { (void)p; return 1; }
size_t LocalSize(void *p) { (void)p; return 0; }
int LocalUnlock(void *p) { (void)p; return 1; }
size_t HeapSize(void *heap, uint32_t flags, void *p) { (void)heap;(void)flags;(void)p;return 0; }
int IsCharAlphaNumericW(uint16_t c) { return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'); }
void *CommandLineToArgvW(const uint16_t *cmd, int *argc) { (void)cmd; if(argc)*argc=0; return 0; }
int FileTimeToLocalFileTime(const void *a, void *b) { if(a&&b) memcpy(b,a,8); return 1; }
int FileTimeToSystemTime(const void *a, void *b) { (void)a;(void)b;return 0; }
void *GetSecurityDescriptorDacl(void *sd, int *present, void **acl, int *defaulted) { (void)sd;if(present)*present=0;if(acl)*acl=0;if(defaulted)*defaulted=0;return 0; }
int CloseClipboard(void) { return 1; }
int EmptyClipboard(void) { return 1; }
int OpenClipboard(void *hwnd) { (void)hwnd; return 1; }
void *GetClipboardData(uint32_t format) { (void)format; return 0; }
void *SetClipboardData(uint32_t format, void *data) { (void)format; return data; }
uint32_t RegisterClipboardFormatW(const uint16_t *name) { (void)name; return 0xC000; }
void *ReleaseStgMedium(void *medium) { (void)medium; return 0; }
void *PathFindNextComponentW(uint16_t *path) { uint16_t *p=path; if(!p)return 0; while(*p&&*p!='/'&&*p!='\\')p++; while(*p=='/'||*p=='\\')p++; return p; }
void *SHDeleteKey(void *root, const uint16_t *subkey) { (void)root;(void)subkey; return (void *)0; }
int DeleteFileW(const uint16_t *path) { (void)path; return 1; }
void *GetEffectiveRightsFromAcl(void *acl, void *trustee, uint32_t *rights) { (void)acl;(void)trustee;if(rights)*rights=0;return 0; }
void *BuildTrusteeWithSid(void *trustee, void *sid) { (void)sid; return trustee; }
void *GetInheritanceSourceW(const uint16_t *name, uint32_t type, uint32_t flags, int container, void *security, uint32_t count, void *objects, void *generic, void *inherit, void *array) { (void)name;(void)type;(void)flags;(void)container;(void)security;(void)count;(void)objects;(void)generic;(void)inherit;(void)array;return 0; }
void FreeInheritedFromArray(void *array, uint32_t count) { (void)array;(void)count; }
int MapGenericMask(uint32_t *access, const void *mapping) { (void)mapping; if(access)*access &= 0xFFFFFFFFu; return 1; }
void *GetNamedSecurityInfoW(const uint16_t *name, uint32_t type, uint32_t info, void **owner, void **group, void **dacl, void **sacl, void **sd) { (void)name;(void)type;(void)info;if(owner)*owner=0;if(group)*group=0;if(dacl)*dacl=0;if(sacl)*sacl=0;if(sd)*sd=0;return (void *)0; }
int OleInitialize(void *reserved) { (void)reserved; return 0; }

static uint32_t wlen(const uint16_t *s) { uint32_t n=0; while(s&&s[n])n++; return n; }
uint16_t *_wcsdup(const uint16_t *s) { uint32_t n=wlen(s)+1,i; uint16_t *p=(uint16_t*)kmalloc(n*2); if(!p)return 0; for(i=0;i<n;i++)p[i]=s[i];return p; }
int _wcsnicmp(const uint16_t *a,const uint16_t *b,size_t n) { size_t i;for(i=0;i<n;i++){uint16_t x=a?a[i]:0,y=b?b[i]:0;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return x<y?-1:1;if(!x)return 0;}return 0; }
uint16_t *wcsncpy(uint16_t *d,const uint16_t *s,size_t n) { size_t i=0;for(;i<n&&s&&s[i];i++)d[i]=s[i];for(;i<n;i++)d[i]=0;return d; }
int wcsncmp(const uint16_t *a,const uint16_t *b,size_t n) { size_t i;for(i=0;i<n;i++){if(a[i]!=b[i])return a[i]<b[i]?-1:1;if(!a[i])break;}return 0; }
uint32_t wcstoul(const uint16_t *s,uint16_t **end,int base) { uint32_t v=0;while(s&&*s>='0'&&*s<='9'){v=v*base+(*s-'0');s++;}if(end)*end=(uint16_t*)s;return v; }
int isdigit(int c){return c>='0'&&c<='9';} int isxdigit(int c){return isdigit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');} int isprint(int c){return c>=32&&c<127;} int iscntrl(int c){return c<32;}
int iswspace(uint16_t c){return c==' '||c=='\t'||c=='\r'||c=='\n';} int iswxdigit(uint16_t c){return isxdigit(c);} uint16_t towlower(uint16_t c){return c>='A'&&c<='Z'?c+32:c;} uint16_t towupper(uint16_t c){return c>='a'&&c<='z'?c-32:c;}
void *strpbrk(const char *s,const char *set){while(s&&*s){const char*p=set;while(p&&*p)if(*s==*p++)return (void*)s;s++;}return 0;}
int qsort_cmp_swap(uint8_t *a,uint8_t *b,size_t n){while(n--){uint8_t t=*a;*a++=*b;*b++=t;}return 0;}
void qsort(void *base,size_t count,size_t size,int(*cmp)(const void*,const void*)){size_t i,j;uint8_t *b=base;for(i=0;i<count;i++)for(j=i+1;j<count;j++)if(cmp(b+i*size,b+j*size)>0)qsort_cmp_swap(b+i*size,b+j*size,size);}
int vswprintf(uint16_t *out,size_t count,const uint16_t *fmt,void *args){(void)fmt;(void)args;if(out&&count)out[0]=0;return 0;}
int _vscwprintf(const uint16_t *fmt,void *args){(void)fmt;(void)args;return 0;}
int _fileno(void *file){(void)file;return -1;} int _setmode(int fd,int mode){(void)fd;return mode;}
void *_wfopen(const uint16_t *name,const uint16_t *mode){(void)name;(void)mode;return 0;} int _wperror(const uint16_t *text){(void)text;return 0;} int fclose(void *f){(void)f;return 0;} int fputs(const char *s,void *f){(void)s;(void)f;return 0;}
