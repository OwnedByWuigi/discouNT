#include <stdint.h>
#include "peloader.h"
#include "mm.h"
#include "util.h"
#include "hal.h"
#include "vga.h"
#include "serial.h"

// Simple stubs that just return 0 or 1
static int stub_ret0(void) { return 0; }
static int stub_ret1(void) { return 1; }
static void stub_void(void) {}

typedef struct {
    const char *dll;
    const char *name;
    void *address;
} API_ENTRY;

static API_ENTRY api_table[] = {
    {"kernel32.dll", "GetModuleHandleA", stub_ret0},
    {"kernel32.dll", "GetModuleHandleW", stub_ret0},
    {"kernel32.dll", "ExitProcess", stub_void},
    {"kernel32.dll", "GetCommandLineA", stub_ret0},
    {"kernel32.dll", "GetCommandLineW", stub_ret0},
    {"kernel32.dll", "GetVersion", stub_ret1},
    {"kernel32.dll", "GetStdHandle", stub_ret0},
    {"kernel32.dll", "WriteConsoleA", stub_ret1},
    {"kernel32.dll", "WriteFile", stub_ret1},
    {"kernel32.dll", "ReadFile", stub_ret1},
    {"kernel32.dll", "SetFilePointer", stub_ret0},
    {"kernel32.dll", "CloseHandle", stub_ret1},
    {"kernel32.dll", "VirtualAlloc", stub_ret0},
    {"kernel32.dll", "VirtualFree", stub_ret1},
    {"kernel32.dll", "VirtualProtect", stub_ret1},
    {"kernel32.dll", "HeapAlloc", stub_ret0},
    {"kernel32.dll", "HeapFree", stub_ret1},
    {"kernel32.dll", "HeapReAlloc", stub_ret0},
    {"kernel32.dll", "GetProcessHeap", stub_ret0},
    {"kernel32.dll", "LoadLibraryA", stub_ret0},
    {"kernel32.dll", "LoadLibraryW", stub_ret0},
    {"kernel32.dll", "GetProcAddress", stub_ret0},
    {"kernel32.dll", "FreeLibrary", stub_ret1},
    {"kernel32.dll", "CreateFileA", stub_ret0},
    {"kernel32.dll", "CreateFileW", stub_ret0},
    {"kernel32.dll", "GetLastError", stub_ret0},
    {"kernel32.dll", "SetLastError", stub_void},
    {"kernel32.dll", "TlsGetValue", stub_ret0},
    {"kernel32.dll", "TlsSetValue", stub_ret1},
    {"kernel32.dll", "TlsAlloc", stub_ret0},
    {"kernel32.dll", "TlsFree", stub_ret1},
    {"kernel32.dll", "MultiByteToWideChar", stub_ret0},
    {"kernel32.dll", "WideCharToMultiByte", stub_ret0},
    {"kernel32.dll", "EnterCriticalSection", stub_void},
    {"kernel32.dll", "LeaveCriticalSection", stub_void},
    {"kernel32.dll", "InitializeCriticalSection", stub_ret1},
    {"kernel32.dll", "InitializeCriticalSectionAndSpinCount", stub_ret1},
    {"kernel32.dll", "DeleteCriticalSection", stub_void},
    {"kernel32.dll", "Sleep", stub_void},
    {"kernel32.dll", "GetTickCount", stub_ret0},
    {"kernel32.dll", "QueryPerformanceCounter", stub_ret1},
    {"kernel32.dll", "QueryPerformanceFrequency", stub_ret1},
    {"kernel32.dll", "SetUnhandledExceptionFilter", stub_ret0},
    {"kernel32.dll", "UnhandledExceptionFilter", stub_ret0},
    {"kernel32.dll", "TerminateProcess", stub_void},
    {"kernel32.dll", "GetCurrentProcess", stub_ret0},
    {"kernel32.dll", "GetCurrentThread", stub_ret0},
    {"kernel32.dll", "GetCurrentProcessId", stub_ret0},
    {"kernel32.dll", "GetCurrentThreadId", stub_ret0},
    {"kernel32.dll", "CreateThread", stub_ret0},
    {"kernel32.dll", "WaitForSingleObject", stub_ret0},
    {"kernel32.dll", "WaitForMultipleObjects", stub_ret0},
    {"kernel32.dll", "SetEvent", stub_ret1},
    {"kernel32.dll", "CreateEventA", stub_ret0},
    {"kernel32.dll", "CreateEventW", stub_ret0},
    {"kernel32.dll", "CreateMutexA", stub_ret0},
    {"kernel32.dll", "CreateMutexW", stub_ret0},
    {"kernel32.dll", "ReleaseMutex", stub_ret1},
    {"kernel32.dll", "GetModuleFileNameA", stub_ret0},
    {"kernel32.dll", "GetModuleFileNameW", stub_ret0},
    {"kernel32.dll", "FindClose", stub_ret1},
    {"kernel32.dll", "FindFirstFileA", stub_ret0},
    {"kernel32.dll", "FindFirstFileW", stub_ret0},
    {"kernel32.dll", "FindNextFileA", stub_ret0},
    {"kernel32.dll", "FindNextFileW", stub_ret0},
    {"kernel32.dll", "GetFileAttributesA", stub_ret0},
    {"kernel32.dll", "GetFileAttributesW", stub_ret0},
    {"kernel32.dll", "SetFileAttributesA", stub_ret0},
    {"kernel32.dll", "SetFileAttributesW", stub_ret0},
    {"kernel32.dll", "DeleteFileA", stub_ret0},
    {"kernel32.dll", "DeleteFileW", stub_ret0},
    {"kernel32.dll", "CreateDirectoryA", stub_ret0},
    {"kernel32.dll", "CreateDirectoryW", stub_ret0},
    {"kernel32.dll", "RemoveDirectoryA", stub_ret0},
    {"kernel32.dll", "RemoveDirectoryW", stub_ret0},
    {"kernel32.dll", "GetFullPathNameA", stub_ret0},
    {"kernel32.dll", "GetFullPathNameW", stub_ret0},
    {"kernel32.dll", "GetTempPathA", stub_ret0},
    {"kernel32.dll", "GetTempPathW", stub_ret0},
    {"kernel32.dll", "GetTempFileNameA", stub_ret0},
    {"kernel32.dll", "GetTempFileNameW", stub_ret0},
    {"kernel32.dll", "MoveFileA", stub_ret0},
    {"kernel32.dll", "MoveFileW", stub_ret0},
    {"kernel32.dll", "CopyFileA", stub_ret0},
    {"kernel32.dll", "CopyFileW", stub_ret0},
    {"kernel32.dll", "GetWindowsDirectoryA", stub_ret0},
    {"kernel32.dll", "GetWindowsDirectoryW", stub_ret0},
    {"kernel32.dll", "GetSystemDirectoryA", stub_ret0},
    {"kernel32.dll", "GetSystemDirectoryW", stub_ret0},
    {"kernel32.dll", "SetCurrentDirectoryA", stub_ret0},
    {"kernel32.dll", "SetCurrentDirectoryW", stub_ret0},
    {"kernel32.dll", "GetCurrentDirectoryA", stub_ret0},
    {"kernel32.dll", "GetCurrentDirectoryW", stub_ret0},
    {"kernel32.dll", "lstrlenA", (void*)strlen},
    {"kernel32.dll", "lstrlenW", stub_ret0},
    {"kernel32.dll", "lstrcpyA", (void*)strcpy},
    {"kernel32.dll", "lstrcpyW", stub_ret0},
    {"kernel32.dll", "lstrcmpA", (void*)strcmp},
    {"kernel32.dll", "lstrcmpW", stub_ret0},
    {"kernel32.dll", "lstrcmpiA", (void*)strcmp},
    {"kernel32.dll", "lstrcmpiW", stub_ret0},
    {"kernel32.dll", "RtlUnwind", stub_void},
    {"kernel32.dll", "RaiseException", stub_void},
    {"kernel32.dll", "IsDebuggerPresent", stub_ret0},
    {"kernel32.dll", "IsProcessorFeaturePresent", stub_ret0},
    {"kernel32.dll", "InterlockedIncrement", stub_ret0},
    {"kernel32.dll", "InterlockedDecrement", stub_ret0},
    {"kernel32.dll", "InterlockedExchange", stub_ret0},
    {"kernel32.dll", "InterlockedCompareExchange", stub_ret0},
    {"kernel32.dll", "FlsAlloc", stub_ret0},
    {"kernel32.dll", "FlsFree", stub_ret1},
    {"kernel32.dll", "FlsGetValue", stub_ret0},
    {"kernel32.dll", "FlsSetValue", stub_ret1},
    {"kernel32.dll", "EncodePointer", stub_ret0},
    {"kernel32.dll", "DecodePointer", stub_ret0},
    {"kernel32.dll", "InitializeSListHead", stub_void},
    {"kernel32.dll", "InterlockedPushEntrySList", stub_ret0},
    {"kernel32.dll", "InterlockedFlushSList", stub_ret0},
    {"user32.dll", "MessageBoxA", stub_ret1},
    {"user32.dll", "MessageBoxW", stub_ret1},
    {"user32.dll", "DefWindowProcA", stub_ret0},
    {"user32.dll", "DefWindowProcW", stub_ret0},
    {"user32.dll", "GetMessageA", stub_ret0},
    {"user32.dll", "GetMessageW", stub_ret0},
    {"user32.dll", "PeekMessageA", stub_ret0},
    {"user32.dll", "PeekMessageW", stub_ret0},
    {"user32.dll", "DispatchMessageA", stub_ret0},
    {"user32.dll", "DispatchMessageW", stub_ret0},
    {"user32.dll", "PostQuitMessage", stub_void},
    {"user32.dll", "RegisterClassA", stub_ret0},
    {"user32.dll", "RegisterClassW", stub_ret0},
    {"user32.dll", "RegisterClassExA", stub_ret0},
    {"user32.dll", "RegisterClassExW", stub_ret0},
    {"user32.dll", "UnregisterClassA", stub_ret1},
    {"user32.dll", "UnregisterClassW", stub_ret1},
    {"user32.dll", "CreateWindowExA", stub_ret0},
    {"user32.dll", "CreateWindowExW", stub_ret0},
    {"user32.dll", "ShowWindow", stub_ret1},
    {"user32.dll", "UpdateWindow", stub_ret1},
    {"user32.dll", "GetClientRect", stub_ret1},
    {"user32.dll", "BeginPaint", stub_ret0},
    {"user32.dll", "EndPaint", stub_ret1},
    {"user32.dll", "LoadIconA", stub_ret0},
    {"user32.dll", "LoadIconW", stub_ret0},
    {"user32.dll", "LoadCursorA", stub_ret0},
    {"user32.dll", "LoadCursorW", stub_ret0},
    {"user32.dll", "LoadMenuA", stub_ret0},
    {"user32.dll", "LoadMenuW", stub_ret0},
    {"user32.dll", "GetSystemMetrics", stub_ret0},
    {"user32.dll", "SetWindowLongA", stub_ret0},
    {"user32.dll", "SetWindowLongW", stub_ret0},
    {"user32.dll", "GetWindowLongA", stub_ret0},
    {"user32.dll", "GetWindowLongW", stub_ret0},
    {"user32.dll", "SendMessageA", stub_ret0},
    {"user32.dll", "SendMessageW", stub_ret0},
    {"user32.dll", "PostMessageA", stub_ret0},
    {"user32.dll", "PostMessageW", stub_ret0},
    {"user32.dll", "TranslateMessage", stub_ret1},
    {"user32.dll", "GetDC", stub_ret0},
    {"user32.dll", "ReleaseDC", stub_ret1},
    {"user32.dll", "InvalidateRect", stub_ret1},
    {"user32.dll", "SetWindowTextA", stub_ret1},
    {"user32.dll", "SetWindowTextW", stub_ret1},
    {"user32.dll", "GetWindowTextA", stub_ret0},
    {"user32.dll", "GetWindowTextW", stub_ret0},
    {"user32.dll", "EnableWindow", stub_ret1},
    {"user32.dll", "DestroyWindow", stub_ret1},
    {"user32.dll", "IsDialogMessageA", stub_ret0},
    {"user32.dll", "IsDialogMessageW", stub_ret0},
    {"user32.dll", "SetFocus", stub_ret0},
    {"user32.dll", "GetFocus", stub_ret0},
    {"user32.dll", "SetCursor", stub_ret0},
    {"user32.dll", "LoadStringA", stub_ret0},
    {"user32.dll", "LoadStringW", stub_ret0},
    {"user32.dll", "CharNextA", stub_ret0},
    {"user32.dll", "CharNextW", stub_ret0},
    {"user32.dll", "wsprintfA", stub_ret0},
    {"user32.dll", "wsprintfW", stub_ret0},
    {"user32.dll", "MessageBeep", stub_void},
    {"user32.dll", "SetTimer", stub_ret0},
    {"user32.dll", "KillTimer", stub_ret1},
    {"user32.dll", "GetKeyState", stub_ret0},
    {"user32.dll", "GetKeyboardState", stub_ret0},
    {"user32.dll", "SetCursorPos", stub_ret1},
    {"user32.dll", "GetCursorPos", stub_ret0},
    {"user32.dll", "ScreenToClient", stub_ret0},
    {"user32.dll", "ClientToScreen", stub_ret0},
    {"user32.dll", "MoveWindow", stub_ret1},
    {"user32.dll", "SetWindowPos", stub_ret1},
    {"user32.dll", "GetWindowRect", stub_ret0},
    {"user32.dll", "AdjustWindowRect", stub_ret0},
    {"user32.dll", "AdjustWindowRectEx", stub_ret0},
    {"user32.dll", "DrawTextA", stub_ret0},
    {"user32.dll", "DrawTextW", stub_ret0},
    {"user32.dll", "FillRect", stub_ret0},
    {"user32.dll", "FrameRect", stub_ret0},
    {"user32.dll", "DrawEdge", stub_ret0},
    {"user32.dll", "DrawFrameControl", stub_ret0},
    {"user32.dll", "DrawIcon", stub_ret0},
    {"user32.dll", "LoadBitmapA", stub_ret0},
    {"user32.dll", "LoadBitmapW", stub_ret0},
    {"user32.dll", "LoadImageA", stub_ret0},
    {"user32.dll", "LoadImageW", stub_ret0},
    {"user32.dll", "CreateDialogParamA", stub_ret0},
    {"user32.dll", "CreateDialogParamW", stub_ret0},
    {"user32.dll", "DialogBoxParamA", stub_ret0},
    {"user32.dll", "DialogBoxParamW", stub_ret0},
    {"user32.dll", "EndDialog", stub_ret1},
    {"user32.dll", "GetDlgItem", stub_ret0},
    {"user32.dll", "SetDlgItemTextA", stub_ret1},
    {"user32.dll", "SetDlgItemTextW", stub_ret1},
    {"user32.dll", "GetDlgItemTextA", stub_ret0},
    {"user32.dll", "GetDlgItemTextW", stub_ret0},
    {"user32.dll", "CheckDlgButton", stub_ret1},
    {"user32.dll", "CheckRadioButton", stub_ret1},
    {"user32.dll", "IsDlgButtonChecked", stub_ret0},
    {"user32.dll", "SendDlgItemMessageA", stub_ret0},
    {"user32.dll", "SendDlgItemMessageW", stub_ret0},
    {"user32.dll", "CallWindowProcA", stub_ret0},
    {"user32.dll", "CallWindowProcW", stub_ret0},
    {"user32.dll", "GetPropA", stub_ret0},
    {"user32.dll", "GetPropW", stub_ret0},
    {"user32.dll", "SetPropA", stub_ret1},
    {"user32.dll", "SetPropW", stub_ret1},
    {"user32.dll", "RemovePropA", stub_ret0},
    {"user32.dll", "RemovePropW", stub_ret0},
    {"user32.dll", "EnumWindows", stub_ret0},
    {"user32.dll", "EnumChildWindows", stub_ret0},
    {"user32.dll", "FindWindowA", stub_ret0},
    {"user32.dll", "FindWindowW", stub_ret0},
    {"user32.dll", "FindWindowExA", stub_ret0},
    {"user32.dll", "FindWindowExW", stub_ret0},
    {"user32.dll", "GetWindow", stub_ret0},
    {"user32.dll", "GetTopWindow", stub_ret0},
    {"user32.dll", "GetDesktopWindow", stub_ret0},
    {"user32.dll", "GetParent", stub_ret0},
    {"user32.dll", "SetParent", stub_ret0},
    {"user32.dll", "IsWindow", stub_ret0},
    {"user32.dll", "IsWindowVisible", stub_ret0},
    {"user32.dll", "IsWindowEnabled", stub_ret0},
    {"user32.dll", "IsChild", stub_ret0},
    {"user32.dll", "WaitMessage", stub_void},
    {"gdi32.dll", "TextOutA", stub_ret1},
    {"gdi32.dll", "TextOutW", stub_ret1},
    {"gdi32.dll", "ExtTextOutA", stub_ret1},
    {"gdi32.dll", "ExtTextOutW", stub_ret1},
    {"gdi32.dll", "GetStockObject", stub_ret0},
    {"gdi32.dll", "SetBkMode", stub_ret1},
    {"gdi32.dll", "SetBkColor", stub_ret0},
    {"gdi32.dll", "SetTextColor", stub_ret0},
    {"gdi32.dll", "DeleteDC", stub_ret1},
    {"gdi32.dll", "CreateCompatibleDC", stub_ret0},
    {"gdi32.dll", "CreateSolidBrush", stub_ret0},
    {"gdi32.dll", "CreateHatchBrush", stub_ret0},
    {"gdi32.dll", "CreatePatternBrush", stub_ret0},
    {"gdi32.dll", "CreateDIBPatternBrush", stub_ret0},
    {"gdi32.dll", "CreatePen", stub_ret0},
    {"gdi32.dll", "CreatePenIndirect", stub_ret0},
    {"gdi32.dll", "SelectObject", stub_ret0},
    {"gdi32.dll", "DeleteObject", stub_ret1},
    {"gdi32.dll", "GetObjectA", stub_ret0},
    {"gdi32.dll", "GetObjectW", stub_ret0},
    {"gdi32.dll", "Rectangle", stub_ret1},
    {"gdi32.dll", "Ellipse", stub_ret1},
    {"gdi32.dll", "RoundRect", stub_ret1},
    {"gdi32.dll", "MoveToEx", stub_ret1},
    {"gdi32.dll", "LineTo", stub_ret1},
    {"gdi32.dll", "Polyline", stub_ret1},
    {"gdi32.dll", "Polygon", stub_ret1},
    {"gdi32.dll", "CreateFontA", stub_ret0},
    {"gdi32.dll", "CreateFontW", stub_ret0},
    {"gdi32.dll", "CreateFontIndirectA", stub_ret0},
    {"gdi32.dll", "CreateFontIndirectW", stub_ret0},
    {"gdi32.dll", "GetDeviceCaps", stub_ret0},
    {"gdi32.dll", "SetMapMode", stub_ret0},
    {"gdi32.dll", "SetViewportOrgEx", stub_ret1},
    {"gdi32.dll", "SetViewportExtEx", stub_ret1},
    {"gdi32.dll", "SetWindowOrgEx", stub_ret1},
    {"gdi32.dll", "SetWindowExtEx", stub_ret1},
    {"gdi32.dll", "BitBlt", stub_ret1},
    {"gdi32.dll", "StretchBlt", stub_ret1},
    {"gdi32.dll", "PatBlt", stub_ret1},
    {"gdi32.dll", "CreateCompatibleBitmap", stub_ret0},
    {"gdi32.dll", "CreateBitmap", stub_ret0},
    {"gdi32.dll", "CreateDIBSection", stub_ret0},
    {"gdi32.dll", "GetDIBits", stub_ret0},
    {"gdi32.dll", "SetDIBits", stub_ret0},
    {"gdi32.dll", "SetPixel", stub_ret0},
    {"gdi32.dll", "GetPixel", stub_ret0},
    {"gdi32.dll", "SaveDC", stub_ret0},
    {"gdi32.dll", "RestoreDC", stub_ret1},
    {"gdi32.dll", "CombineRgn", stub_ret0},
    {"gdi32.dll", "CreateRectRgn", stub_ret0},
    {"gdi32.dll", "GetClipBox", stub_ret0},
    {"gdi32.dll", "SelectClipRgn", stub_ret0},
    {"gdi32.dll", "OffsetClipRgn", stub_ret0},
    {"gdi32.dll", "GetTextExtentPoint32A", stub_ret0},
    {"gdi32.dll", "GetTextExtentPoint32W", stub_ret0},
    {"gdi32.dll", "GetTextMetricsA", stub_ret0},
    {"gdi32.dll", "GetTextMetricsW", stub_ret0},
    {"msvcrt.dll", "printf", stub_ret0},
    {"msvcrt.dll", "puts", stub_ret0},
    {"msvcrt.dll", "exit", stub_void},
    {"msvcrt.dll", "_exit", stub_void},
    {"msvcrt.dll", "abort", stub_void},
    {"msvcrt.dll", "malloc", stub_ret0},
    {"msvcrt.dll", "free", stub_void},
    {"msvcrt.dll", "calloc", stub_ret0},
    {"msvcrt.dll", "realloc", stub_ret0},
    {"msvcrt.dll", "fopen", stub_ret0},
    {"msvcrt.dll", "fclose", stub_ret0},
    {"msvcrt.dll", "fread", stub_ret0},
    {"msvcrt.dll", "fwrite", stub_ret0},
    {"msvcrt.dll", "fprintf", stub_ret0},
    {"msvcrt.dll", "sprintf", stub_ret0},
    {"msvcrt.dll", "strcmp", (void*)strcmp},
    {"msvcrt.dll", "strcpy", (void*)strcpy},
    {"msvcrt.dll", "strlen", (void*)strlen},
    {"msvcrt.dll", "memset", (void*)memset},
    {"msvcrt.dll", "memcpy", (void*)memcpy},
    {"msvcrt.dll", "memcmp", stub_ret0},
    {"msvcrt.dll", "memmove", stub_ret0},
    {"msvcrt.dll", "strchr", stub_ret0},
    {"msvcrt.dll", "strrchr", stub_ret0},
    {"msvcrt.dll", "strstr", stub_ret0},
    {"msvcrt.dll", "strcat", stub_ret0},
    {"msvcrt.dll", "strncat", stub_ret0},
    {"msvcrt.dll", "strncpy", stub_ret0},
    {"msvcrt.dll", "strncmp", stub_ret0},
    {"msvcrt.dll", "atoi", stub_ret0},
    {"msvcrt.dll", "atol", stub_ret0},
    {"msvcrt.dll", "atof", stub_ret0},
    {"msvcrt.dll", "tolower", stub_ret0},
    {"msvcrt.dll", "toupper", stub_ret0},
    {"msvcrt.dll", "isalpha", stub_ret0},
    {"msvcrt.dll", "isdigit", stub_ret0},
    {"msvcrt.dll", "isspace", stub_ret0},
    {"msvcrt.dll", "islower", stub_ret0},
    {"msvcrt.dll", "isupper", stub_ret0},
    {"msvcrt.dll", "rand", stub_ret0},
    {"msvcrt.dll", "srand", stub_void},
    {"msvcrt.dll", "time", stub_ret0},
    {"msvcrt.dll", "clock", stub_ret0},
    {"msvcrt.dll", "atexit", stub_ret0},
    {"msvcrt.dll", "system", stub_ret0},
    {"msvcrt.dll", "getenv", stub_ret0},
    {"msvcrt.dll", "__getmainargs", stub_void},
    {"msvcrt.dll", "__p__environ", stub_ret0},
    {"msvcrt.dll", "__p__fmode", stub_ret0},
    {"msvcrt.dll", "__set_app_type", stub_void},
    {"msvcrt.dll", "__setusermatherr", stub_void},
    {"msvcrt.dll", "_cexit", stub_void},
    {"msvcrt.dll", "_c_exit", stub_void},
    {"msvcrt.dll", "_exit", stub_void},
    {"msvcrt.dll", "_XcptFilter", stub_ret0},
    {"msvcrt.dll", "_acmdln", stub_ret0},
    {"msvcrt.dll", "_initterm", stub_void},
    {"msvcrt.dll", "__C_specific_handler", stub_ret0},
    {"msvcrt.dll", "??2@YAPAXI@Z", stub_ret0},  // operator new
    {"msvcrt.dll", "??3@YAXPAX@Z", stub_void},  // operator delete
    {"msvcrt.dll", "??_U@YAPAXI@Z", stub_ret0},  // operator new[]
    {"msvcrt.dll", "??_V@YAXPAX@Z", stub_void},  // operator delete[]
    {0, 0, 0}
};

static void *FindApi(const char *dll, const char *func) {
    for (int i = 0; api_table[i].dll; i++) {
        if (strcmp(api_table[i].dll, dll) == 0 && 
            strcmp(api_table[i].name, func) == 0) {
            return api_table[i].address;
        }
    }
    return stub_ret0; // Return safe stub if not found
}

void *PeLoadImage(void *image_data, uint32_t size) {
    SerialPutString("[PE] PeLoadImage: size=");
    SerialPrintDec(size);
    SerialPutString("\r\n");
    
    if (size < 64) { SerialPutString("[PE] FAIL: Too small\r\n"); return 0; }
    
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_data;
    if (dos->e_magic != 0x5A4D) { SerialPutString("[PE] FAIL: Bad MZ\r\n"); return 0; }
    
    if (dos->e_lfanew + 4 > size) { SerialPutString("[PE] FAIL: PE offset OOB\r\n"); return 0; }
    
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_data + dos->e_lfanew);
    if (file->Signature != 0x00004550) { SerialPutString("[PE] FAIL: Bad PE sig\r\n"); return 0; }
    if (file->Machine != 0x14C) { SerialPutString("[PE] FAIL: Not i386\r\n"); return 0; }
    
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    if (opt->Magic != 0x10B) { SerialPutString("[PE] FAIL: Not PE32\r\n"); return 0; }
    
    uint32_t image_size = opt->SizeOfImage;
    if (image_size == 0 || image_size > 0x1000000) { SerialPutString("[PE] FAIL: Bad image size\r\n"); return 0; }
    
    SerialPutString("[PE] Base=0x");
    SerialPrintHex(opt->ImageBase);
    SerialPutString(" Size=0x");
    SerialPrintHex(image_size);
    SerialPutString("\r\n");
    
    uint8_t *image_base = (uint8_t*)kmalloc(image_size);
    if (!image_base) { SerialPutString("[PE] FAIL: OOM\r\n"); return 0; }
    
    SerialPutString("[PE] Alloc at 0x");
    SerialPrintHex((uint32_t)image_base);
    SerialPutString("\r\n");
    
    memset(image_base, 0, image_size);
    
    uint32_t headers_size = opt->SizeOfHeaders;
    if (headers_size > size) headers_size = size;
    memcpy(image_base, image_data, headers_size);
    
    IMAGE_SECTION_HEADER *sections = (IMAGE_SECTION_HEADER*)((uint8_t*)opt + file->SizeOfOptionalHeader);
    
    for (int i = 0; i < file->NumberOfSections; i++) {
        if (sections[i].SizeOfRawData > 0) {
            uint32_t dest = sections[i].VirtualAddress;
            uint32_t src = sections[i].PointerToRawData;
            uint32_t len = sections[i].SizeOfRawData;
            
            if (src + len <= size && dest + len <= image_size) {
                memcpy(image_base + dest, (uint8_t*)image_data + src, len);
            }
        }
    }
    
    SerialPutString("[PE] Load OK\r\n");
    return image_base;
}

void PePerformRelocations(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    int32_t delta = (int32_t)((uint32_t)image_base - opt->ImageBase);
    
    SerialPutString("[PE] Reloc delta=0x");
    SerialPrintHex((uint32_t)delta);
    SerialPutString("\r\n");
    
    if (delta == 0) { SerialPutString("[PE] No reloc needed\r\n"); return; }
    
    if (opt->NumberOfRvaAndSizes <= 5) { SerialPutString("[PE] No reloc dir\r\n"); return; }
    
    uint32_t *data_dir = (uint32_t*)((uint8_t*)opt + 96);
    uint32_t reloc_rva = data_dir[10];
    uint32_t reloc_size = data_dir[11];
    
    if (reloc_rva == 0 || reloc_size == 0) { SerialPutString("[PE] No relocs\r\n"); return; }
    
    uint8_t *reloc = (uint8_t*)image_base + reloc_rva;
    uint8_t *reloc_end = reloc + reloc_size;
    int count = 0;
    
    while (reloc < reloc_end) {
        uint32_t page_rva = *(uint32_t*)reloc;
        uint32_t block_size = *(uint32_t*)(reloc + 4);
        if (block_size == 0) break;
        
        uint16_t *entries = (uint16_t*)(reloc + 8);
        int n = (block_size - 8) / 2;
        
        for (int i = 0; i < n; i++) {
            uint16_t entry = entries[i];
            if ((entry >> 12) == 3) {
                uint32_t *patch = (uint32_t*)((uint8_t*)image_base + page_rva + (entry & 0xFFF));
                *patch = (uint32_t)((int32_t)(*patch) + delta);
                count++;
            }
        }
        reloc += block_size;
    }
    
    SerialPutString("[PE] Applied ");
    SerialPrintDec(count);
    SerialPutString(" relocs\r\n");
}

void *PeGetEntryPoint(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    return (uint8_t*)image_base + opt->AddressOfEntryPoint;
}

int PeResolveImports(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    if (opt->NumberOfRvaAndSizes < 2) { SerialPutString("[PE] No imports (NumDataDirs<2)\r\n"); return 1; }
    
    uint32_t *data_dir = (uint32_t*)((uint8_t*)opt + 96);
    uint32_t import_rva = data_dir[2];
    uint32_t import_size = data_dir[3];
    
    SerialPutString("[PE] Import RVA=0x");
    SerialPrintHex(import_rva);
    SerialPutString("\r\n");
    
    if (import_rva == 0) { SerialPutString("[PE] No imports\r\n"); return 1; }
    
    IMAGE_IMPORT_DESCRIPTOR *import = (IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)image_base + import_rva);
    int missing = 0;
    int line_y = 200;
    
    while (import->Name != 0) {
        if (import->Name >= opt->SizeOfImage) break;
        
        const char *dll_name = (const char*)((uint8_t*)image_base + import->Name);
        SerialPutString("[PE] DLL: ");
        SerialPutString(dll_name);
        SerialPutString("\r\n");
        
        uint32_t lookup_rva = import->ImportLookupTable;
        if (lookup_rva == 0) lookup_rva = import->ImportAddressTable;
        if (lookup_rva == 0) { import++; continue; }
        
        uint32_t *lookup = (uint32_t*)((uint8_t*)image_base + lookup_rva);
        uint32_t *iat = (uint32_t*)((uint8_t*)image_base + import->ImportAddressTable);
        
        while (*lookup != 0) {
            const char *func_name = 0;
            
            if (*lookup & 0x80000000) {
                SerialPutString("  Ordinal: ");
                SerialPrintDec(*lookup & 0xFFFF);
                SerialPutString("\r\n");
            } else {
                uint32_t name_rva = *lookup & 0x7FFFFFFF;
                if (name_rva < opt->SizeOfImage) {
                    uint16_t *hint = (uint16_t*)((uint8_t*)image_base + name_rva);
                    func_name = (const char*)((uint8_t*)hint + 2);
                    SerialPutString("  ");
                    SerialPutString(func_name);
                }
            }
            
            void *api_addr = 0;
            if (func_name) api_addr = FindApi(dll_name, func_name);
            if (!api_addr) api_addr = stub_ret0;
            
            *iat = (uint32_t)api_addr;
            
            if (func_name && FindApi(dll_name, func_name)) {
                SerialPutString(" -> OK\r\n");
            } else if (func_name) {
                missing++;
                SerialPutString(" -> STUB\r\n");
                char msg[128];
                strcpy(msg, "STUB: ");
                strcat(msg, dll_name);
                strcat(msg, "!");
                strcat(msg, func_name);
                if (line_y < 460) {
                    VgaDrawString(10, line_y, msg, COLOR_YELLOW, COLOR_RED);
                    line_y += 12;
                }
            }
            
            lookup++;
            iat++;
        }
        import++;
    }
    
    SerialPutString("[PE] Done. Stubs: ");
    SerialPrintDec(missing);
    SerialPutString("\r\n");
    return 1;
}

void PePrintInfo(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    char buf[64];
    int y = 300;
    
    VgaDrawString(10, y, "PE Loaded - Entry: 0x", COLOR_WHITE, COLOR_BLUE);
    itoa(opt->AddressOfEntryPoint, buf, 16);
    VgaDrawString(200, y, buf, COLOR_WHITE, COLOR_BLUE);
}