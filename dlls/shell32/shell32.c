#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

extern int MessageBoxW(void *hwnd, const uint16_t *text, const uint16_t *caption, uint32_t type);

__attribute__((stdcall)) int Shell_NotifyIconW(uint32_t dwMessage, void *lpData) {
    (void)dwMessage;
    (void)lpData;
    return 1;
}

__attribute__((stdcall)) int ShellAboutW(void *hWnd, const uint16_t *szApp, const uint16_t *szOtherStuff, void *hIcon) {
    (void)hIcon;
    return MessageBoxW(hWnd, szOtherStuff ? szOtherStuff : (const uint16_t*)L"", szApp ? szApp : (const uint16_t*)L"About", 0);
}
