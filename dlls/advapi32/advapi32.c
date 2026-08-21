#include <stdint.h>
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

int RegOpenKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t ulOptions, uint32_t samDesired, void **phkResult) {
    (void)hKey;
    (void)lpSubKey;
    (void)ulOptions;
    (void)samDesired;
    if (phkResult) *phkResult = (void*)0x100;
    return 2;
}

int RegOpenKeyW(void *hKey, const uint16_t *lpSubKey, void **phkResult) {
    return RegOpenKeyExW(hKey, lpSubKey, 0, 0, phkResult);
}

int RegCreateKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t Reserved, uint16_t *lpClass,
                                             uint32_t dwOptions, uint32_t samDesired, void *lpSecurityAttributes,
                                             void **phkResult, uint32_t *lpdwDisposition) {
    (void)hKey; (void)lpSubKey; (void)Reserved; (void)lpClass; (void)dwOptions;
    (void)samDesired; (void)lpSecurityAttributes;
    if (phkResult) *phkResult = (void*)0x101;
    if (lpdwDisposition) *lpdwDisposition = 1;
    return 0;
}

int RegQueryValueExW(void *hKey, const uint16_t *lpValueName, uint32_t *lpReserved, uint32_t *lpType,
                                              uint8_t *lpData, uint32_t *lpcbData) {
    (void)hKey; (void)lpValueName; (void)lpReserved;
    if (lpType) *lpType = 3;
    if (lpData && lpcbData) {
        uint32_t i;
        for (i = 0; i < *lpcbData; i++) lpData[i] = 0;
    }
    return 2;
}

int RegSetValueExW(void *hKey, const uint16_t *lpValueName, uint32_t Reserved, uint32_t dwType,
                                            const uint8_t *lpData, uint32_t cbData) {
    (void)hKey; (void)lpValueName; (void)Reserved; (void)dwType; (void)lpData; (void)cbData;
    return 0;
}

int RegSetValueExA(void *key, const char *name, uint32_t reserved,
                                            uint32_t type, const uint8_t *data, uint32_t bytes) {
    (void)key;(void)name;(void)reserved;(void)type;(void)data;(void)bytes;return 0;
}

int RegGetValueW(void *key,const uint16_t *subkey,const uint16_t *value,
                                          uint32_t flags,uint32_t *type,void *data,uint32_t *bytes) {
    (void)subkey;(void)flags;return RegQueryValueExW(key,value,0,type,(uint8_t*)data,bytes);
}

int RegCloseKey(void *hKey) {
    (void)hKey;
    return 0;
}

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
