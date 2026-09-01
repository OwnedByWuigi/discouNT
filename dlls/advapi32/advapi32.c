#include <stdint.h>
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern char *strcpy(char *dest, const char *src);
extern char *strcat(char *dest, const char *src);
extern uint32_t strlen(const char *str);
typedef uint16_t WCHAR;
typedef const WCHAR *LPCWSTR;
typedef WCHAR *LPWSTR;
typedef uint32_t DWORD;
typedef int BOOL;
typedef void *SC_HANDLE;
typedef struct _SERVICE_STATUS {
    DWORD dwServiceType, dwCurrentState, dwControlsAccepted, dwWin32ExitCode;
    DWORD dwServiceSpecificExitCode, dwCheckPoint, dwWaitHint;
} SERVICE_STATUS, *LPSERVICE_STATUS;
#define SERVICE_STOPPED 1
#define SERVICE_RUNNING 4
#define SERVICE_DISABLED 4
#define SERVICE_CONTROL_STOP 1
#define REG_HKLM ((void*)(uintptr_t)0x80000002U)

extern int RegCreateKeyExW(void *, const uint16_t *, uint32_t, uint16_t *, uint32_t, uint32_t, void *, void **, uint32_t *);
extern int RegSetValueExW(void *, const uint16_t *, uint32_t, uint32_t, const uint8_t *, uint32_t);
extern int RegOpenKeyW(void *, const uint16_t *, void **);
extern int RegQueryValueExW(void *, const uint16_t *, uint32_t *, uint32_t *, uint8_t *, uint32_t *);

typedef struct _SC_SERVICE_RECORD {
    WCHAR name[64];
    SERVICE_STATUS status;
    int used;
} SC_SERVICE_RECORD;

static SC_SERVICE_RECORD service_records[32];
static int service_count;

static SC_SERVICE_RECORD *find_service(LPCWSTR name) {
    int i, j;
    for (i = 0; i < service_count; i++) {
        for (j = 0; j < 63 && service_records[i].name[j] && name && service_records[i].name[j] == name[j]; j++);
        if (name && service_records[i].name[j] == name[j]) return &service_records[i];
    }
    return 0;
}

static int service_ascii_to_wide(const char *src, uint16_t *dst, int limit) {
    int i;
    for (i = 0; src && src[i] && i + 1 < limit; i++) dst[i] = (uint8_t)src[i];
    if (i >= limit) return 0;
    dst[i] = 0;
    return 1;
}

static void service_registry_set_dword(void *key, const char *name, uint32_t value) {
    uint16_t wide_name[64];
    if (service_ascii_to_wide(name, wide_name, 64))
        RegSetValueExW(key, wide_name, 0, 4, (const uint8_t*)&value, sizeof(value));
}

static void service_registry_set_string(void *key, const char *name, LPCWSTR value) {
    uint16_t wide_name[64];
    uint32_t bytes = 0;
    if (!service_ascii_to_wide(name, wide_name, 64) || !value) return;
    while (value[bytes / 2]) bytes += 2;
    bytes += 2;
    RegSetValueExW(key, wide_name, 0, 1, (const uint8_t*)value, bytes);
}

static void service_registry_store(LPCWSTR name, LPCWSTR display, DWORD type, DWORD start,
                                   DWORD error, LPCWSTR binary, LPCWSTR depend) {
    uint16_t path[256];
    uint16_t prefix[] = {'S','Y','S','T','E','M','\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\','S','e','r','v','i','c','e','s','\\',0};
    int i = 0, j;
    void *key = 0;
    while (prefix[i] && i + 1 < 256) { path[i] = prefix[i]; i++; }
    for (j = 0; name && name[j] && i + 1 < 256; j++) path[i++] = name[j];
    path[i] = 0;
    if (RegCreateKeyExW(REG_HKLM, path, 0, 0, 0, 0, 0, &key, 0) != 0) return;
    service_registry_set_dword(key, "Type", type);
    service_registry_set_dword(key, "Start", start);
    service_registry_set_dword(key, "ErrorControl", error);
    service_registry_set_string(key, "DisplayName", display);
    service_registry_set_string(key, "ImagePath", binary);
    if (depend) service_registry_set_string(key, "DependOnService", depend);
}

SC_HANDLE OpenSCManagerW(LPCWSTR machine, LPCWSTR database, DWORD access) {
    (void)machine; (void)database; (void)access;
    return (SC_HANDLE)(uintptr_t)0x53434D01;
}

SC_HANDLE OpenServiceW(SC_HANDLE manager, LPCWSTR name, DWORD access) {
    (void)manager; (void)access;
    return (SC_HANDLE)find_service(name);
}

SC_HANDLE CreateServiceW(SC_HANDLE manager, LPCWSTR name, LPCWSTR display, DWORD access,
                         DWORD type, DWORD start, DWORD error, LPCWSTR binary, LPCWSTR group,
                         DWORD *tag, LPCWSTR depend, LPCWSTR object, LPCWSTR password) {
    SC_SERVICE_RECORD *record;
    int i;
    (void)manager; (void)display; (void)access; (void)binary; (void)group;
    (void)tag; (void)depend; (void)object; (void)password;
    if (!name || service_count >= 32 || find_service(name)) return 0;
    record = &service_records[service_count++];
    for (i = 0; i < 63 && name[i]; i++) record->name[i] = name[i];
    record->name[i] = 0;
    record->used = 1;
    record->status.dwServiceType = type;
    record->status.dwCurrentState = start == SERVICE_DISABLED ? SERVICE_STOPPED : SERVICE_STOPPED;
    record->status.dwWin32ExitCode = error;
    service_registry_store(name, display, type, start, error, binary, depend);
    return (SC_HANDLE)record;
}

BOOL CloseServiceHandle(SC_HANDLE service) { (void)service; return 1; }
BOOL DeleteService(SC_HANDLE service) {
    SC_SERVICE_RECORD *record = (SC_SERVICE_RECORD*)service;
    if (!record || !record->used) return 0;
    record->used = 0;
    return 1;
}
BOOL StartServiceW(SC_HANDLE service, DWORD argc, LPCWSTR *argv) {
    SC_SERVICE_RECORD *record = (SC_SERVICE_RECORD*)service;
    (void)argc; (void)argv;
    if (!record || !record->used) return 0;
    record->status.dwCurrentState = SERVICE_RUNNING;
    return 1;
}
BOOL ControlService(SC_HANDLE service, DWORD control, LPSERVICE_STATUS status) {
    SC_SERVICE_RECORD *record = (SC_SERVICE_RECORD*)service;
    if (!record || !record->used) return 0;
    if (control == SERVICE_CONTROL_STOP) record->status.dwCurrentState = SERVICE_STOPPED;
    if (status) *status = record->status;
    return 1;
}
BOOL QueryServiceStatus(SC_HANDLE service, LPSERVICE_STATUS status) {
    SC_SERVICE_RECORD *record = (SC_SERVICE_RECORD*)service;
    if (!record || !record->used || !status) return 0;
    *status = record->status;
    return 1;
}
BOOL ChangeServiceConfig2W(SC_HANDLE service, DWORD level, void *info) {
    (void)service; (void)level; (void)info; return 1;
}

int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

#define ERROR_SUCCESS       0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_INVALID_HANDLE 6
#define ERROR_MORE_DATA      234
#define ERROR_NO_MORE_ITEMS  259
#define ERROR_OUTOFMEMORY    14
#define ERROR_INVALID_PARAMETER 87
#define REG_CREATED_NEW_KEY 1
#define REG_OPENED_EXISTING_KEY 2
#define REG_SZ               1
#define REG_EXPAND_SZ        2
#define REG_BINARY           3
#define REG_DWORD            4
#define REG_MULTI_SZ         7
#define HKEY_CLASSES_ROOT    ((void*)(uintptr_t)0x80000000U)
#define HKEY_CURRENT_USER    ((void*)(uintptr_t)0x80000001U)
#define HKEY_LOCAL_MACHINE   ((void*)(uintptr_t)0x80000002U)
#define HKEY_USERS           ((void*)(uintptr_t)0x80000003U)
#define REG_MAX_KEYS         128
#define REG_MAX_VALUES       256
#define REG_MAX_PATH         256
#define REG_MAX_NAME         96
#define REG_MAX_DATA         1024

typedef struct {
    int used;
    void *root;
    char path[REG_MAX_PATH];
} REG_KEY;

typedef struct {
    int used;
    REG_KEY *key;
    char name[REG_MAX_NAME];
    uint32_t type;
    uint32_t size;
    uint8_t data[REG_MAX_DATA];
} REG_VALUE;

static REG_KEY registry_keys[REG_MAX_KEYS];
static REG_VALUE registry_values[REG_MAX_VALUES];
static int registry_initialized;
static REG_VALUE *reg_find_value(REG_KEY *key, const char *name);
static void registry_seed_services(void);

static int reg_ascii_equal(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
    }
    return *a == 0 && *b == 0;
}

static int reg_wide_to_ascii(const uint16_t *src, char *dst, int limit) {
    int i = 0;
    if (!dst || limit < 1) return 0;
    if (!src) { dst[0] = 0; return 1; }
    while (src[i] && i + 1 < limit) {
        dst[i] = src[i] < 128 ? (char)src[i] : '?';
        i++;
    }
    dst[i] = 0;
    return src[i] == 0;
}

static void reg_root_name(void *root, char *out) {
    if (root == HKEY_CURRENT_USER) { strcpy(out, "HKCU"); return; }
    if (root == HKEY_USERS) { strcpy(out, "HKU"); return; }
    if (root == HKEY_CLASSES_ROOT) { strcpy(out, "HKCR"); return; }
    strcpy(out, "HKLM");
}

static int reg_path_from(void *root, const char *subkey, char *out) {
    char root_name[8];
    int pos, i;
    if (!out) return 0;
    reg_root_name(root, root_name);
    strcpy(out, root_name);
    pos = (int)strlen(out);
    if (subkey && *subkey) {
        if (pos + 1 >= REG_MAX_PATH) return 0;
        out[pos++] = '\\';
        for (i = 0; subkey[i] && pos + 1 < REG_MAX_PATH; i++) {
            out[pos++] = subkey[i] == '/' ? '\\' : subkey[i];
        }
        if (subkey[i]) return 0;
    }
    out[pos] = 0;
    return 1;
}

static REG_KEY *reg_find_key(const char *path) {
    int i;
    for (i = 0; i < REG_MAX_KEYS; i++)
        if (registry_keys[i].used && reg_ascii_equal(registry_keys[i].path, path)) return &registry_keys[i];
    return 0;
}

static REG_KEY *reg_make_key(void *root, const char *path) {
    int i;
    REG_KEY *key = reg_find_key(path);
    if (key) return key;
    for (i = 0; i < REG_MAX_KEYS; i++) if (!registry_keys[i].used) {
        key = &registry_keys[i];
        key->used = 1; key->root = root; strcpy(key->path, path);
        return key;
    }
    return 0;
}

static void registry_init(void) {
    if (registry_initialized) return;
    registry_initialized = 1;
    reg_make_key(HKEY_LOCAL_MACHINE, "HKLM");
    reg_make_key(HKEY_CURRENT_USER, "HKCU");
    reg_make_key(HKEY_USERS, "HKU");
    reg_make_key(HKEY_CLASSES_ROOT, "HKCR");
    registry_seed_services();
}

int RegOpenKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t ulOptions, uint32_t samDesired, void **phkResult) {
    char subkey[REG_MAX_PATH], path[REG_MAX_PATH];
    (void)ulOptions; (void)samDesired;
    registry_init();
    if (!phkResult || !reg_wide_to_ascii(lpSubKey, subkey, sizeof(subkey))) return ERROR_INVALID_PARAMETER;
    if (hKey && hKey != HKEY_CURRENT_USER && hKey != HKEY_LOCAL_MACHINE &&
        hKey != HKEY_USERS && hKey != HKEY_CLASSES_ROOT) {
        REG_KEY *parent = (REG_KEY*)hKey;
        if (!parent->used) return ERROR_INVALID_HANDLE;
        strcpy(path, parent->path);
        if (subkey[0]) { strcat(path, "\\"); strcat(path, subkey); }
    } else if (!reg_path_from(hKey, subkey, path)) return ERROR_INVALID_PARAMETER;
    *phkResult = reg_find_key(path);
    return *phkResult ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
}

int RegOpenKeyW(void *hKey, const uint16_t *lpSubKey, void **phkResult) {
    return RegOpenKeyExW(hKey, lpSubKey, 0, 0, phkResult);
}

int RegCreateKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t Reserved, uint16_t *lpClass,
                   uint32_t dwOptions, uint32_t samDesired, void *lpSecurityAttributes,
                   void **phkResult, uint32_t *lpdwDisposition) {
    char subkey[REG_MAX_PATH], path[REG_MAX_PATH];
    REG_KEY *key;
    (void)Reserved; (void)lpClass; (void)dwOptions; (void)samDesired; (void)lpSecurityAttributes;
    registry_init();
    if (!phkResult || !reg_wide_to_ascii(lpSubKey, subkey, sizeof(subkey))) return ERROR_INVALID_PARAMETER;
    if (hKey && hKey != HKEY_CURRENT_USER && hKey != HKEY_LOCAL_MACHINE &&
        hKey != HKEY_USERS && hKey != HKEY_CLASSES_ROOT) {
        REG_KEY *parent = (REG_KEY*)hKey;
        if (!parent->used) return ERROR_INVALID_HANDLE;
        strcpy(path, parent->path);
        if (subkey[0]) { strcat(path, "\\"); strcat(path, subkey); }
    } else if (!reg_path_from(hKey, subkey, path)) return ERROR_INVALID_PARAMETER;
    key = reg_find_key(path);
    if (lpdwDisposition) *lpdwDisposition = key ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
    if (!key) key = reg_make_key(hKey, path);
    if (!key) return ERROR_OUTOFMEMORY;
    *phkResult = key;
    return ERROR_SUCCESS;
}

static REG_VALUE *reg_find_value(REG_KEY *key, const char *name) {
    int i;
    for (i = 0; i < REG_MAX_VALUES; i++)
        if (registry_values[i].used && registry_values[i].key == key && reg_ascii_equal(registry_values[i].name, name)) return &registry_values[i];
    return 0;
}

static void registry_seed_service(const char *name, uint32_t type, uint32_t start) {
    char path[REG_MAX_PATH];
    REG_KEY *key;
    REG_VALUE *value;
    int i;
    strcpy(path, "HKLM\\SYSTEM\\CurrentControlSet\\Services\\");
    strcat(path, name);
    key = reg_make_key(HKEY_LOCAL_MACHINE, path);
    if (!key) return;
    value = reg_find_value(key, "Type");
    if (!value) for (i = 0; i < REG_MAX_VALUES; i++) if (!registry_values[i].used) {
        value = &registry_values[i]; value->used = 1; value->key = key; strcpy(value->name, "Type"); break;
    }
    if (value) { value->type = REG_DWORD; value->size = 4; memcpy(value->data, &type, 4); }
    value = reg_find_value(key, "Start");
    if (!value) for (i = 0; i < REG_MAX_VALUES; i++) if (!registry_values[i].used) {
        value = &registry_values[i]; value->used = 1; value->key = key; strcpy(value->name, "Start"); break;
    }
    if (value) { value->type = REG_DWORD; value->size = 4; memcpy(value->data, &start, 4); }
}

static void registry_seed_services(void) {
    reg_make_key(HKEY_LOCAL_MACHINE, "HKLM\\SYSTEM");
    reg_make_key(HKEY_LOCAL_MACHINE, "HKLM\\SYSTEM\\CurrentControlSet");
    reg_make_key(HKEY_LOCAL_MACHINE, "HKLM\\SYSTEM\\CurrentControlSet\\Services");
    registry_seed_service("Serial", 1, 0);
    registry_seed_service("Vga", 1, 1);
    registry_seed_service("Keyboard", 1, 1);
    registry_seed_service("Framebuffer", 1, 1);
    registry_seed_service("Mouse", 1, 1);
    registry_seed_service("Net", 1, 2);
    registry_seed_service("AC97", 1, 1);
    registry_seed_service("HDA", 1, 1);
    registry_seed_service("ES1371", 1, 1);
    registry_seed_service("SB16", 1, 1);
}

int RegQueryValueExW(void *hKey, const uint16_t *lpValueName, uint32_t *lpReserved, uint32_t *lpType,
                    uint8_t *lpData, uint32_t *lpcbData) {
    char name[REG_MAX_NAME];
    REG_VALUE *value;
    REG_KEY *key = (REG_KEY*)hKey;
    (void)lpReserved;
    registry_init();
    if (!key || !key->used || !lpcbData || !reg_wide_to_ascii(lpValueName, name, sizeof(name))) return ERROR_INVALID_PARAMETER;
    value = reg_find_value(key, name);
    if (!value) return ERROR_FILE_NOT_FOUND;
    if (lpType) *lpType = value->type;
    if (!lpData) { *lpcbData = value->size; return ERROR_SUCCESS; }
    if (*lpcbData < value->size) { *lpcbData = value->size; return ERROR_MORE_DATA; }
    memcpy(lpData, value->data, value->size);
    *lpcbData = value->size;
    return ERROR_SUCCESS;
}

int RegSetValueExW(void *hKey, const uint16_t *lpValueName, uint32_t Reserved, uint32_t dwType,
                   const uint8_t *lpData, uint32_t cbData) {
    char name[REG_MAX_NAME];
    REG_VALUE *value;
    REG_KEY *key = (REG_KEY*)hKey;
    int i;
    (void)Reserved;
    registry_init();
    if (!key || !key->used || cbData > REG_MAX_DATA || !reg_wide_to_ascii(lpValueName, name, sizeof(name))) return ERROR_INVALID_PARAMETER;
    value = reg_find_value(key, name);
    if (!value) for (i = 0; i < REG_MAX_VALUES; i++) if (!registry_values[i].used) {
        value = &registry_values[i]; value->used = 1; value->key = key; strcpy(value->name, name); break;
    }
    if (!value) return ERROR_OUTOFMEMORY;
    value->type = dwType; value->size = cbData;
    if (cbData && lpData) memcpy(value->data, lpData, cbData);
    return ERROR_SUCCESS;
}

int RegSetValueExA(void *key, const char *name, uint32_t reserved, uint32_t type, const uint8_t *data, uint32_t bytes) {
    uint16_t wide[REG_MAX_NAME]; int i;
    if (!name) return ERROR_INVALID_PARAMETER;
    for (i = 0; name[i] && i + 1 < REG_MAX_NAME; i++) wide[i] = (uint8_t)name[i];
    wide[i] = 0;
    return RegSetValueExW(key, wide, reserved, type, data, bytes);
}

int RegGetValueW(void *key, const uint16_t *subkey, const uint16_t *value,
                 uint32_t flags, uint32_t *type, void *data, uint32_t *bytes) {
    void *opened = key;
    int result;
    (void)flags;
    if (subkey && *subkey) {
        result = RegOpenKeyW(key, subkey, &opened);
        if (result) return result;
    }
    return RegQueryValueExW(opened, value, 0, type, (uint8_t*)data, bytes);
}

int RegCloseKey(void *hKey) { (void)hKey; return ERROR_SUCCESS; }

static uint32_t reg_ascii_to_wide(const char *src, uint16_t *dst, uint32_t limit) {
    uint32_t i = 0;
    if (!dst || !limit) return 0;
    while (src && src[i] && i + 1 < limit) { dst[i] = (uint8_t)src[i]; i++; }
    dst[i] = 0;
    return i;
}

static int reg_prefix_equal(const char *a, const char *b, uint32_t count) {
    uint32_t i; for (i = 0; i < count; i++) if (a[i] != b[i]) return 0; return 1;
}
static int reg_has_separator_after(const char *text) {
    while (text && *text) { if (*text == '\\') return 1; text++; } return 0;
}

int RegCreateKeyW(void *key, const uint16_t *name, void **result) {
    return RegCreateKeyExW(key, name, 0, 0, 0, 0, 0, result, 0);
}

int RegDeleteValueW(void *hKey, const uint16_t *valueName) {
    char name[REG_MAX_NAME];
    REG_VALUE *value;
    if (!hKey || !reg_wide_to_ascii(valueName, name, sizeof(name))) return ERROR_INVALID_PARAMETER;
    value = reg_find_value((REG_KEY*)hKey, name);
    if (!value) return ERROR_FILE_NOT_FOUND;
    value->used = 0;
    return ERROR_SUCCESS;
}

int RegFlushKey(void *hKey) { (void)hKey; registry_init(); return ERROR_SUCCESS; }

int RegQueryInfoKeyW(void *hKey, uint16_t *className, uint32_t *classLen,
                     uint32_t *reserved, uint32_t *subKeys, uint32_t *maxSubKeyLen,
                     uint32_t *maxClassLen, uint32_t *values, uint32_t *maxValueNameLen,
                     uint32_t *maxValueLen, uint32_t *securityLen, void *lastWrite) {
    REG_KEY *key = (REG_KEY*)hKey; int i, count = 0, valueCount = 0; uint32_t maxName = 0, maxValue = 0;
    (void)className; (void)classLen; (void)reserved; (void)maxClassLen; (void)securityLen; (void)lastWrite;
    if (!key || !key->used) return ERROR_INVALID_HANDLE;
    for (i = 0; i < REG_MAX_KEYS; i++) if (registry_keys[i].used && registry_keys[i].root == key->root) {
        const char *p = registry_keys[i].path; uint32_t n = strlen(key->path);
        if (reg_prefix_equal(p, key->path, n) && p[n] == '\\' && !reg_has_separator_after(p + n + 1)) count++;
    }
    for (i = 0; i < REG_MAX_VALUES; i++) if (registry_values[i].used && registry_values[i].key == key) {
        uint32_t n = strlen(registry_values[i].name); if (n > maxName) maxName = n; if (registry_values[i].size > maxValue) maxValue = registry_values[i].size; valueCount++;
    }
    if (subKeys) *subKeys = count; if (maxSubKeyLen) *maxSubKeyLen = 0;
    if (values) *values = valueCount; if (maxValueNameLen) *maxValueNameLen = maxName; if (maxValueLen) *maxValueLen = maxValue;
    return ERROR_SUCCESS;
}

int RegEnumValueW(void *hKey, uint32_t index, uint16_t *name, uint32_t *nameLen,
                 uint32_t *reserved, uint32_t *type, uint8_t *data, uint32_t *dataLen) {
    REG_KEY *key = (REG_KEY*)hKey; int i; uint32_t seen = 0, needed;
    (void)reserved;
    if (!key || !key->used || !nameLen) return ERROR_INVALID_PARAMETER;
    for (i = 0; i < REG_MAX_VALUES; i++) if (registry_values[i].used && registry_values[i].key == key) {
        if (seen++ != index) continue;
        needed = strlen(registry_values[i].name);
        if (*nameLen <= needed) { *nameLen = needed; return ERROR_MORE_DATA; }
        reg_ascii_to_wide(registry_values[i].name, name, *nameLen);
        *nameLen = needed;
        if (type) *type = registry_values[i].type;
        if (dataLen) { if (!data || *dataLen < registry_values[i].size) { *dataLen = registry_values[i].size; return data ? ERROR_MORE_DATA : ERROR_SUCCESS; } memcpy(data, registry_values[i].data, registry_values[i].size); *dataLen = registry_values[i].size; }
        return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_ITEMS;
}

int RegEnumKeyExW(void *hKey, uint32_t index, uint16_t *name, uint32_t *nameLen,
                  uint32_t *reserved, uint16_t *className, uint32_t *classLen, void *lastWrite) {
    REG_KEY *key = (REG_KEY*)hKey; int i; uint32_t seen = 0, n, prefix;
    (void)reserved; (void)className; (void)classLen; (void)lastWrite;
    if (!key || !key->used || !name || !nameLen) return ERROR_INVALID_PARAMETER;
    prefix = strlen(key->path);
    for (i = 0; i < REG_MAX_KEYS; i++) if (registry_keys[i].used && registry_keys[i].root == key->root && reg_prefix_equal(registry_keys[i].path, key->path, prefix) && registry_keys[i].path[prefix] == '\\') {
        const char *child = registry_keys[i].path + prefix + 1;
        if (reg_has_separator_after(child)) continue;
        if (seen++ != index) continue;
        n = strlen(child); if (*nameLen <= n) { *nameLen = n; return ERROR_MORE_DATA; }
        reg_ascii_to_wide(child, name, *nameLen); *nameLen = n; return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_ITEMS;
}

int RegDeleteTreeW(void *hKey, const uint16_t *subKey) { (void)hKey; (void)subKey; return ERROR_SUCCESS; }
int RegLoadKeyW(void *hKey, const uint16_t *subKey, const uint16_t *file) { (void)hKey; (void)subKey; (void)file; return ERROR_SUCCESS; }
int RegRestoreKey(void *hKey, const uint16_t *file, uint32_t flags) { (void)hKey; (void)file; (void)flags; return ERROR_SUCCESS; }
int RegSaveKeyW(void *hKey, const uint16_t *file, void *security) { (void)hKey; (void)file; (void)security; return ERROR_SUCCESS; }
int RegUnLoadKeyW(void *hKey, const uint16_t *subKey) { (void)hKey; (void)subKey; return ERROR_SUCCESS; }

int OpenProcessToken(void *ProcessHandle, uint32_t DesiredAccess, void **TokenHandle) {
    (void)ProcessHandle; (void)DesiredAccess;
    if (TokenHandle) *TokenHandle = (void*)0x200;
    return 1;
}

int AdjustTokenPrivileges(void *TokenHandle, int DisableAllPrivileges,
                                                   void *NewState, uint32_t BufferLength,
                                                   void *PreviousState, uint32_t *ReturnLength) {
    (void)TokenHandle; (void)DisableAllPrivileges; (void)NewState;
    (void)BufferLength; (void)PreviousState;
    if (ReturnLength) *ReturnLength = 0;
    return 1;
}

int LookupPrivilegeValueW(const uint16_t *lpSystemName, const uint16_t *lpName, void *lpLuid) {
    (void)lpSystemName; (void)lpName;
    if (lpLuid) {
        uint32_t *v = (uint32_t*)lpLuid;
        v[0] = 0;
        v[1] = 0;
    }
    return 1;
}

int GetUserNameW(uint16_t *lpBuffer, uint32_t *pcbBuffer) {
    static const uint16_t name[] = { 'A','d','m','i','n','i','s','t','r','a','t','o','r',0 };
    uint32_t need = sizeof(name) / sizeof(name[0]);
    uint32_t i;
    if (!pcbBuffer) return 0;
    if (!lpBuffer || *pcbBuffer < need) {
        *pcbBuffer = need;
        return 0;
    }
    for (i = 0; i < need; i++) lpBuffer[i] = name[i];
    *pcbBuffer = need - 1;
    return 1;
}
