#include <stdint.h>
#include <windows.h>
#include <security.h>
#include <sddl.h>

/* The WINE utility expects the normal advapi32/security32 surface.  These
 * small implementations expose discouNT's current local Administrator
 * identity while keeping the application itself unchanged. */
static const WCHAR g_user[] = L"Administrator";
static const WCHAR g_domain[] = L"DISCOUNT";
static const WCHAR g_sid_text[] = L"S-1-5-21-0-0-0-500";

static int wlen(const WCHAR *s) { int n = 0; while (s && s[n]) n++; return n; }
static void wcopy(WCHAR *d, const WCHAR *s, ULONG cap) {
    ULONG i = 0;
    if (!d || !cap) return;
    while (s && s[i] && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = 0;
}

int WINAPI _wcsicmp(LPCWSTR a, LPCWSTR b) {
    while (a && b && *a && *b) {
        WCHAR ca = *a++, cb = *b++;
        if (ca >= L'A' && ca <= L'Z') ca += L'a' - L'A';
        if (cb >= L'A' && cb <= L'Z') cb += L'a' - L'A';
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    return *a == *b ? 0 : (*a ? 1 : -1);
}

void *malloc(SIZE_T size) { return HeapAlloc(GetProcessHeap(), 0, size); }
void free(void *ptr) { if (ptr) HeapFree(GetProcessHeap(), 0, ptr); }

BOOL WINAPI GetUserNameExW(EXTENDED_NAME_FORMAT format, LPWSTR name, PULONG size) {
    WCHAR value[96];
    ULONG need;
    int i;
    if (!size) return FALSE;
    if (format == NameUserPrincipal) {
        wcopy(value, g_user, 96);
        i = wlen(value); value[i++] = L'@'; wcopy(value + i, g_domain, 96 - i);
    } else if (format == NameSamCompatible) {
        wcopy(value, g_domain, 96);
        i = wlen(value); value[i++] = L'\\'; wcopy(value + i, g_user, 96 - i);
    } else {
        wcopy(value, g_user, 96);
    }
    need = (ULONG)wlen(value) + 1;
    if (!name || *size < need) { *size = need; SetLastError(ERROR_MORE_DATA); return FALSE; }
    wcopy(name, value, *size); *size = need - 1; return TRUE;
}

BOOL WINAPI ConvertSidToStringSidW(PSID sid, LPWSTR *string_sid) {
    int i;
    (void)sid;
    if (!string_sid) return FALSE;
    *string_sid = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(g_sid_text));
    if (!*string_sid) return FALSE;
    for (i = 0; g_sid_text[i]; i++) (*string_sid)[i] = g_sid_text[i];
    (*string_sid)[i] = 0;
    return TRUE;
}

DWORD WINAPI GetLengthSid(PSID sid) { (void)sid; return 12; }
BOOL WINAPI CopySid(DWORD length, PSID dst, PSID src) {
    BYTE *d = (BYTE *)dst, *s = (BYTE *)src;
    DWORD i;
    if (!d || !s || length < 12) return FALSE;
    for (i = 0; i < length; i++) d[i] = s[i];
    return TRUE;
}

BOOL WINAPI GetTokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS cls,
                                LPVOID info, DWORD len, PDWORD ret) {
    static SID sid = { 1, 1, {{0, 0, 0, 0, 0, 5}}, {500} };
    (void)token;
    if (cls == TokenUser) {
        if (ret) *ret = sizeof(TOKEN_USER);
        if (!info || len < sizeof(TOKEN_USER)) { SetLastError(ERROR_MORE_DATA); return FALSE; }
        ((PTOKEN_USER)info)->User.Sid = &sid; ((PTOKEN_USER)info)->User.Attributes = 0;
        return TRUE;
    }
    if (cls == TokenGroups) {
        if (ret) *ret = sizeof(TOKEN_GROUPS);
        if (!info || len < sizeof(TOKEN_GROUPS)) { SetLastError(ERROR_MORE_DATA); return FALSE; }
        ((PTOKEN_GROUPS)info)->GroupCount = 1;
        ((PTOKEN_GROUPS)info)->Groups[0].Sid = &sid;
        ((PTOKEN_GROUPS)info)->Groups[0].Attributes = SE_GROUP_LOGON_ID;
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI WriteConsoleW(HANDLE handle, LPCVOID buffer, DWORD length,
                          PDWORD written, LPVOID reserved) {
    char out[256]; const WCHAR *in = (const WCHAR *)buffer; DWORD i, n;
    (void)reserved;
    if (!in) return FALSE;
    n = length < sizeof(out) - 1 ? length : sizeof(out) - 1;
    for (i = 0; i < n; i++) out[i] = in[i] < 128 ? (char)in[i] : '?';
    out[n] = 0;
    if (!WriteFile(handle, out, n, written, NULL)) return FALSE;
    return TRUE;
}

UINT WINAPI GetOEMCP(void) { return 437; }
