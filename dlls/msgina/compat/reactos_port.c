/* Native dependency layer used by the ReactOS MSGINA core.  These are small
 * Win32/LSA semantics backed by discouNT's object and session primitives. */
#include "../msgina.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *p);
extern void *memcpy(void *, const void *, uint32_t);
extern void *memset(void *, int, uint32_t);

static DWORD last_error;
DWORD WINAPI GetLastError(void) { return last_error; }
void WINAPI SetLastError(DWORD e) { last_error = e; }
void WINAPI ZeroMemory(void *p, SIZE_T n) { memset(p, 0, (uint32_t)n); }
void WINAPI SecureZeroMemory(void *p, SIZE_T n) { memset(p, 0, (uint32_t)n); }

SIZE_T WINAPI wcslen(LPCWSTR s) { SIZE_T n = 0; while (s && s[n]) n++; return n; }
LPWSTR WINAPI wcscpy(LPWSTR d, LPCWSTR s) { LPWSTR p=d; while ((*p++=*s++)); return d; }
WCHAR *wcschr(const WCHAR *s, WCHAR c) { while (s && *s) { if (*s==c) return (WCHAR *)s; s++; } return (s && c==0) ? (WCHAR *)s : NULL; }
int WINAPI _wcsicmp(LPCWSTR a, LPCWSTR b) {
    while (*a && *b) { WCHAR x=*a++, y=*b++; if (x>='a'&&x<='z') x-=32; if (y>='a'&&y<='z') y-=32; if (x!=y) return x-y; }
    return *a-*b;
}
LPWSTR WINAPI wcscat(LPWSTR d, LPCWSTR s) { LPWSTR p=d; while (*p) p++; while ((*p++=*s++)); return d; }
int WINAPI wcscmp(LPCWSTR a, LPCWSTR b) { while (*a && *a==*b) { a++; b++; } return *a-*b; }
LPWSTR WINAPI DuplicateString(LPCWSTR s) {
    SIZE_T n; LPWSTR p; if (!s) return NULL; n=(wcslen(s)+1)*sizeof(WCHAR); p=kmalloc((uint32_t)n); if (p) memcpy(p,s,(uint32_t)n); return p;
}

HANDLE WINAPI GetProcessHeap(void) { return (HANDLE)1; }
LPVOID WINAPI HeapAlloc(HANDLE h, DWORD flags, SIZE_T n) { LPVOID p; (void)h; p=kmalloc((uint32_t)n); if (p && (flags&HEAP_ZERO_MEMORY)) memset(p,0,(uint32_t)n); return p; }
BOOL WINAPI HeapFree(HANDLE h, DWORD flags, LPVOID p) { (void)h;(void)flags; if(p) kfree(p); return TRUE; }
HLOCAL WINAPI LocalAlloc(UINT flags, SIZE_T n) { return (HLOCAL)HeapAlloc((HANDLE)1, flags, n); }
HLOCAL WINAPI LocalFree(HLOCAL p) { if(p) kfree(p); return NULL; }
BOOL WINAPI CloseHandle(HANDLE h) { (void)h; return TRUE; }

LONG WINAPI RegOpenKeyExW(HKEY root,LPCWSTR sub,DWORD opt,DWORD access,HKEY *out) { (void)root;(void)sub;(void)opt;(void)access; if(out)*out=(HKEY)1; return ERROR_SUCCESS; }
LONG WINAPI RegOpenKeyW(HKEY r,LPCWSTR s,HKEY *o) { return RegOpenKeyExW(r,s,0,0,o); }
LONG WINAPI RegCreateKeyExW(HKEY r,LPCWSTR s,DWORD a,LPWSTR c,DWORD o,DWORD d,void *sa,HKEY *k,DWORD *disp) { (void)r;(void)s;(void)a;(void)c;(void)o;(void)d;(void)sa; if(k)*k=(HKEY)1;if(disp)*disp=1;return ERROR_SUCCESS; }
LONG WINAPI RegQueryValueExW(HKEY k,LPCWSTR n,DWORD *r,DWORD *t,LPBYTE d,DWORD *cb) { (void)k;(void)n;(void)r;(void)t;(void)d;(void)cb; return ERROR_FILE_NOT_FOUND; }
LONG WINAPI RegSetValueExW(HKEY k,LPCWSTR n,DWORD r,DWORD t,const BYTE *d,DWORD cb) { (void)k;(void)n;(void)r;(void)t;(void)d;(void)cb;return ERROR_SUCCESS; }
LONG WINAPI RegCloseKey(HKEY k) { (void)k; return ERROR_SUCCESS; }
LONG RegOpenLoggedOnHKCU(HANDLE t, REGSAM a, PHKEY k) { (void)t;(void)a;if(k)*k=NULL;return ERROR_FILE_NOT_FOUND; }
LONG ReadRegSzValue(HKEY k,LPCWSTR n,LPWSTR *v) { (void)k;(void)n;if(v)*v=NULL;return ERROR_FILE_NOT_FOUND; }
LONG ReadRegDwordValue(HKEY k,LPCWSTR n,PDWORD v) { (void)k;(void)n;if(v)*v=0;return ERROR_FILE_NOT_FOUND; }

HMODULE WINAPI LoadLibraryW(LPCWSTR n) { (void)n; return NULL; }
FARPROC WINAPI GetProcAddress(HMODULE m,LPCSTR n) { (void)m;(void)n;return NULL; }
DWORD WINAPI GetWindowsDirectoryW(LPWSTR b,DWORD n) { static const WCHAR x[]={'/','S','Y','S','T','E','M','3','2',0}; SIZE_T i=0; while(i+1<n&&x[i]){b[i]=x[i];i++;}if(n)b[i]=0;return (DWORD)i; }
DWORD WINAPI ExpandEnvironmentStringsW(LPCWSTR s,LPWSTR d,DWORD n) { SIZE_T i=0;while(s&&s[i]&&i+1<n){d[i]=s[i];i++;}if(n)d[i]=0;return (DWORD)(i+1); }
BOOL WINAPI DuplicateTokenEx(HANDLE e,DWORD a,LPSECURITY_ATTRIBUTES s,SECURITY_IMPERSONATION_LEVEL l,TOKEN_TYPE t,PHANDLE o) { (void)a;(void)s;(void)l;(void)t;if(o)*o=e;return TRUE; }
BOOL WINAPI CreateProcessAsUserW(HANDLE t,LPCWSTR a,LPWSTR c,LPSECURITY_ATTRIBUTES p,LPSECURITY_ATTRIBUTES q,BOOL i,DWORD f,LPVOID e,LPCWSTR d,LPSTARTUPINFOW s,LPPROCESS_INFORMATION pi) { (void)t;(void)a;(void)c;(void)p;(void)q;(void)i;(void)f;(void)e;(void)d;(void)s;if(pi)ZeroMemory(pi,sizeof(*pi));return FALSE; }

BOOL WINAPI GetTokenInformation(HANDLE t,TOKEN_INFORMATION_CLASS c,LPVOID p,DWORD n,PDWORD r) {
    if (c == TokenStatistics) {
        TOKEN_STATISTICS *stats = (TOKEN_STATISTICS *)p;
        if (r) *r = sizeof(TOKEN_STATISTICS);
        if (!p || n < sizeof(TOKEN_STATISTICS)) {
            SetLastError(122);
            return FALSE;
        }
        ZeroMemory(stats, sizeof(*stats));
        stats->TokenId.LowPart = 1;
        stats->AuthenticationId.LowPart = 1;
        return TRUE;
    }
    if (r) *r = 0;
    SetLastError(122);
    return FALSE;
}
HANDLE WINAPI OpenThreadToken(HANDLE t,DWORD a,BOOL s,PHANDLE o) { (void)t;(void)a;(void)s;if(o)*o=NULL;SetLastError(1008);return NULL; }
HANDLE WINAPI GetCurrentThread(void) { return (HANDLE)1; }
BOOL WINAPI AllocateLocallyUniqueId(PLUID l) { static DWORD x=1;if(l){l->LowPart=x++;l->HighPart=0;}return TRUE; }
BOOL WINAPI GetComputerNameW(LPWSTR n,PDWORD sz) { static const WCHAR x[]={'D','I','S','C','O','U','N','T',0};SIZE_T i=0;if(!sz)return FALSE;while(i+1<*sz&&x[i]){n[i]=x[i];i++;}n[i]=0;*sz=(DWORD)i;return TRUE; }
BOOL WINAPI RtlEqualSid(PSID a,PSID b) { return a==b; }
PSID WINAPI RtlAllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY a,BYTE c,DWORD s1,DWORD s2,DWORD s3,DWORD s4,DWORD s5,DWORD s6,DWORD s7,DWORD s8,PSID *o) { (void)a;(void)c;(void)s1;(void)s2;(void)s3;(void)s4;(void)s5;(void)s6;(void)s7;(void)s8;if(o)*o=kmalloc(16);return o?*o:NULL; }
PVOID WINAPI RtlFreeSid(PSID p) { if(p)kfree(p);return NULL; }
SHORT WINAPI GetKeyState(int k) { (void)k;return 0; }
int WINAPI GetSystemMetrics(int i) { (void)i;return 0; }
void WINAPI GetLocalTime(LPSYSTEMTIME t) { if(t)ZeroMemory(t,sizeof(*t)); }

NTSTATUS ConnectToLsa(PGINA_CONTEXT c) { if(c)c->LsaHandle=(HANDLE)1; if(c)c->AuthenticationPackage=0; return STATUS_SUCCESS; }
NTSTATUS MyLogonUser(HANDLE l,ULONG p,LPWSTR u,LPWSTR d,LPWSTR pw,PHANDLE tok,PNTSTATUS sub) { (void)l;(void)p;(void)u;(void)d;(void)pw;if(tok)*tok=(HANDLE)1;if(sub)*sub=STATUS_SUCCESS;return STATUS_SUCCESS; }
NTSTATUS WINAPI LsaOpenPolicy(PLSA_UNICODE_STRING n,PLSA_OBJECT_ATTRIBUTES a,ACCESS_MASK m,LSA_HANDLE *h){(void)n;(void)a;(void)m;if(h)*h=(HANDLE)1;return STATUS_SUCCESS;}
NTSTATUS WINAPI LsaRetrievePrivateData(LSA_HANDLE h,PLSA_UNICODE_STRING n,PLSA_UNICODE_STRING *d){(void)h;(void)n;if(d)*d=NULL;return STATUS_NOT_IMPLEMENTED;}
NTSTATUS WINAPI LsaFreeMemory(PVOID p){(void)p;return STATUS_SUCCESS;}
NTSTATUS WINAPI LsaClose(LSA_HANDLE h){(void)h;return STATUS_SUCCESS;}
NTSTATUS WINAPI LsaDeregisterLogonProcess(LSA_HANDLE h){(void)h;return STATUS_SUCCESS;}
NTSTATUS WINAPI NtQueryInformationToken(HANDLE t,TOKEN_INFORMATION_CLASS c,PVOID p,ULONG n,PULONG r){(void)t;(void)c;(void)p;(void)n;if(r)*r=0;return STATUS_BUFFER_TOO_SMALL;}
NTSTATUS WINAPI LsaCallAuthenticationPackage(LSA_HANDLE h,ULONG p,PVOID q,ULONG n,PVOID *r,PULONG rn,PNTSTATUS ps){(void)h;(void)p;(void)q;(void)n;if(r)*r=NULL;if(rn)*rn=0;if(ps)*ps=STATUS_NOT_IMPLEMENTED;return STATUS_NOT_IMPLEMENTED;}
NTSTATUS WINAPI LsaFreeReturnBuffer(PVOID p){(void)p;return STATUS_SUCCESS;}
NTSTATUS WINAPI RtlAdjustPrivilege(ULONG p,BOOLEAN e,BOOLEAN t,BOOLEAN *o){(void)p;(void)e;(void)t;if(o)*o=0;return STATUS_SUCCESS;}
NTSTATUS WINAPI NtShutdownSystem(ULONG a){(void)a;return STATUS_NOT_IMPLEMENTED;}
void WINAPI RtlInitUnicodeString(PUNICODE_STRING d,PCWSTR s){if(!d)return;d->Buffer=(PWSTR)s;d->Length=(USHORT)(wcslen(s)*sizeof(WCHAR));d->MaximumLength=d->Length+sizeof(WCHAR);}
HRESULT WINAPI StringCbCopyNExW(LPWSTR d,SIZE_T db,LPCWSTR s,SIZE_T sb,LPWSTR *e,SIZE_T *r,DWORD f){SIZE_T n=sb/sizeof(WCHAR),i; (void)f;for(i=0;i<n&&i+1<db/sizeof(WCHAR);i++)d[i]=s[i];if(db)d[i<db/sizeof(WCHAR)?i:db/sizeof(WCHAR)-1]=0;if(e)*e=d+i;if(r)*r=(db/sizeof(WCHAR)>i?db/sizeof(WCHAR)-i:0);return S_OK;}
HRESULT WINAPI StringCbPrintfW(LPWSTR d,SIZE_T db,LPCWSTR fmt,...){(void)fmt;if(db)d[0]=0;return S_OK;}

BOOL TestTokenPrivilege(HANDLE token, ULONG privilege) { (void)token; (void)privilege; return FALSE; }
DWORD LoadShutdownSelState(HKEY key) { (void)key; return 0; }
void SaveShutdownSelState(HKEY key, DWORD code) { (void)key; (void)code; }
DWORD GetAllowedShutdownOptions(HKEY key, HANDLE token) { (void)key; (void)token; return WLX_SHUTDOWN_STATE_POWER_OFF | WLX_SHUTDOWN_STATE_REBOOT; }
INT_PTR ShutdownDialog(HWND hwnd, DWORD options, PGINA_CONTEXT context) { (void)hwnd; (void)options; (void)context; return 0; }
int WINAPI wsprintfW(LPWSTR dst, LPCWSTR fmt, ...) { (void)fmt; if (dst) dst[0]=0; return 0; }
int WINAPI GetObjectW(HGDIOBJ object, int bytes, LPVOID buffer) {
    BITMAP *bm = (BITMAP *)buffer;
    uintptr_t id = (uintptr_t)object & 0xFFFFu;
    if (!bm || bytes < (int)sizeof(BITMAP)) return 0;
    ZeroMemory(bm, sizeof(*bm));
    if (id == IDI_ROSLOGO) {
        bm->bmWidth = 275; bm->bmHeight = 54;
    } else if (id == IDI_BAR) {
        bm->bmWidth = 275; bm->bmHeight = 4;
    } else {
        return 0;
    }
    bm->bmPlanes = 1;
    bm->bmBitsPixel = 32;
    return sizeof(*bm);
}
UINT WINAPI GetUserDefaultLangID(void) { return 0x0409; }
BOOL WINAPI AdjustWindowRectEx(LPRECT r, DWORD style, BOOL menu, DWORD ex) { (void)style;(void)menu;(void)ex;return r != NULL; }
BOOL WINAPI DuplicateHandle(HANDLE sp,HANDLE s,HANDLE tp,HANDLE *out,DWORD a,BOOL i,DWORD o) { (void)sp;(void)tp;(void)a;(void)i;(void)o;if(out)*out=s;return TRUE; }
BOOL WINAPI SetThreadDesktop(HDESK d) { (void)d; return TRUE; }
BOOL WINAPI MoveWindow(HWND h,int x,int y,int w,int height,BOOL repaint) { (void)h;(void)x;(void)y;(void)w;(void)height;(void)repaint;return TRUE; }
