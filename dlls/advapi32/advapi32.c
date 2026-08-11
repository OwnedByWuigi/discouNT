#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

__attribute__((stdcall)) int RegOpenKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t ulOptions, uint32_t samDesired, void **phkResult) {
    (void)hKey;
    (void)lpSubKey;
    (void)ulOptions;
    (void)samDesired;
    if (phkResult) *phkResult = (void*)0x100;
    return 2;
}

__attribute__((stdcall)) int RegCreateKeyExW(void *hKey, const uint16_t *lpSubKey, uint32_t Reserved, uint16_t *lpClass,
                                             uint32_t dwOptions, uint32_t samDesired, void *lpSecurityAttributes,
                                             void **phkResult, uint32_t *lpdwDisposition) {
    (void)hKey; (void)lpSubKey; (void)Reserved; (void)lpClass; (void)dwOptions;
    (void)samDesired; (void)lpSecurityAttributes;
    if (phkResult) *phkResult = (void*)0x101;
    if (lpdwDisposition) *lpdwDisposition = 1;
    return 0;
}

__attribute__((stdcall)) int RegQueryValueExW(void *hKey, const uint16_t *lpValueName, uint32_t *lpReserved, uint32_t *lpType,
                                              uint8_t *lpData, uint32_t *lpcbData) {
    (void)hKey; (void)lpValueName; (void)lpReserved;
    if (lpType) *lpType = 3;
    if (lpData && lpcbData) {
        uint32_t i;
        for (i = 0; i < *lpcbData; i++) lpData[i] = 0;
    }
    return 2;
}

__attribute__((stdcall)) int RegSetValueExW(void *hKey, const uint16_t *lpValueName, uint32_t Reserved, uint32_t dwType,
                                            const uint8_t *lpData, uint32_t cbData) {
    (void)hKey; (void)lpValueName; (void)Reserved; (void)dwType; (void)lpData; (void)cbData;
    return 0;
}

__attribute__((stdcall)) int RegCloseKey(void *hKey) {
    (void)hKey;
    return 0;
}

__attribute__((stdcall)) int OpenProcessToken(void *ProcessHandle, uint32_t DesiredAccess, void **TokenHandle) {
    (void)ProcessHandle; (void)DesiredAccess;
    if (TokenHandle) *TokenHandle = (void*)0x200;
    return 1;
}

__attribute__((stdcall)) int AdjustTokenPrivileges(void *TokenHandle, int DisableAllPrivileges,
                                                   void *NewState, uint32_t BufferLength,
                                                   void *PreviousState, uint32_t *ReturnLength) {
    (void)TokenHandle; (void)DisableAllPrivileges; (void)NewState;
    (void)BufferLength; (void)PreviousState;
    if (ReturnLength) *ReturnLength = 0;
    return 1;
}

__attribute__((stdcall)) int LookupPrivilegeValueW(const uint16_t *lpSystemName, const uint16_t *lpName, void *lpLuid) {
    (void)lpSystemName; (void)lpName;
    if (lpLuid) {
        uint32_t *v = (uint32_t*)lpLuid;
        v[0] = 0;
        v[1] = 0;
    }
    return 1;
}

__attribute__((stdcall)) int GetUserNameW(uint16_t *lpBuffer, uint32_t *pcbBuffer) {
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
