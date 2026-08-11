#ifndef DISCOUNT_WINDOWS_H
#define DISCOUNT_WINDOWS_H

#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "winuser.h"
#include "wingdi.h"

typedef struct tagNMHDR NMHDR;
typedef struct tagMINMAXINFO MINMAXINFO;

typedef struct _STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    BYTE *lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);

typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        };
    };
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef struct _OSVERSIONINFOW {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVERSIONINFOW, *LPOSVERSIONINFOW;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef struct _CPINFOEXW {
    UINT MaxCharSize;
    BYTE DefaultChar[2];
    BYTE LeadByte[12];
    WCHAR UnicodeDefaultChar;
    UINT CodePage;
    WCHAR CodePageName[MAX_PATH];
} CPINFOEXW, *LPCPINFOEXW;

DWORD WINAPI GetLastError(void);
void WINAPI SetLastError(DWORD dwErrCode);
HANDLE WINAPI OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
BOOL WINAPI CloseHandle(HANDLE hObject);
DWORD WINAPI GetCurrentProcessId(void);
HANDLE WINAPI GetCurrentProcess(void);
void WINAPI InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void WINAPI LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
BOOL WINAPI OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, HANDLE *TokenHandle);
BOOL WINAPI AdjustTokenPrivileges(HANDLE TokenHandle, BOOL DisableAllPrivileges,
                                  PTOKEN_PRIVILEGES NewState, DWORD BufferLength,
                                  PTOKEN_PRIVILEGES PreviousState, DWORD *ReturnLength);
BOOL WINAPI SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass);
DWORD WINAPI GetPriorityClass(HANDLE hProcess);
BOOL WINAPI LookupPrivilegeValueW(LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid);
BOOL WINAPI GetProcessAffinityMask(HANDLE hProcess, DWORD_PTR *lpProcessAffinityMask, DWORD_PTR *lpSystemAffinityMask);
BOOL WINAPI SetProcessAffinityMask(HANDLE hProcess, DWORD_PTR dwProcessAffinityMask);
BOOL WINAPI TerminateProcess(HANDLE hProcess, UINT uExitCode);
BOOL WINAPI ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead);
BOOL WINAPI WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten);
DWORD WINAPI GetGuiResources(HANDLE hProcess, DWORD uiFlags);
BOOL WINAPI GetProcessIoCounters(HANDLE hProcess, PIO_COUNTERS lpIoCounters);
BOOL WINAPI IsWow64Process(HANDLE hProcess, BOOL *Wow64Process);
BOOL WINAPI ImpersonateLoggedOnUser(HANDLE hToken);
BOOL WINAPI RevertToSelf(void);
BOOL WINAPI GetUserNameW(LPWSTR lpBuffer, DWORD *pcbBuffer);
int WINAPI MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                               LPWSTR lpWideCharStr, int cchWideChar);
int WINAPI WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                               LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, BOOL *lpUsedDefaultChar);
BOOL WINAPI GetCPInfoExW(UINT CodePage, DWORD dwFlags, LPCPINFOEXW lpCPInfoEx);
HANDLE WINAPI CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
BOOL WINAPI SetEvent(HANDLE hEvent);
BOOL WINAPI ResetEvent(HANDLE hEvent);
DWORD WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
HANDLE WINAPI CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                           LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                           DWORD dwCreationFlags, DWORD *lpThreadId);
BOOL WINAPI CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                           LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                           BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                           LPCWSTR lpCurrentDirectory, STARTUPINFOW *lpStartupInfo,
                           PROCESS_INFORMATION *lpProcessInformation);
HANDLE WINAPI CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL WINAPI ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                     DWORD *lpNumberOfBytesRead, LPVOID lpOverlapped);
BOOL WINAPI WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                      DWORD *lpNumberOfBytesWritten, LPVOID lpOverlapped);
DWORD WINAPI GetFileSize(HANDLE hFile, DWORD *lpFileSizeHigh);
BOOL WINAPI SetEndOfFile(HANDLE hFile);
HANDLE WINAPI FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
BOOL WINAPI FindClose(HANDLE hFindFile);
void WINAPI GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
BOOL WINAPI GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);
void WINAPI Sleep(DWORD dwMilliseconds);
DWORD WINAPI GetTickCount(void);
DWORD WINAPI GetVersion(void);
HANDLE WINAPI GetProcessHeap(void);
LPVOID WINAPI HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
BOOL WINAPI HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
HMODULE WINAPI GetModuleHandleW(LPCWSTR lpModuleName);
HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
FARPROC WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HANDLE WINAPI LoadImageA(HINSTANCE hinst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);
LPWSTR WINAPI GetCommandLineW(void);
HACCEL WINAPI LoadAcceleratorsW(HINSTANCE hInstance, LPCWSTR lpTableName);
int WINAPI TranslateAcceleratorW(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg);
BOOL WINAPI IsDialogMessageW(HWND hDlg, LPMSG lpMsg);
int WINAPI lstrlenW(LPCWSTR lpString);
WCHAR *wcsupr(WCHAR *str);
WCHAR *_ui64tow(ULONGLONG value, WCHAR *buffer, int radix);
LPWSTR WINAPI lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2);
LPWSTR WINAPI lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength);
LPWSTR WINAPI lstrcatW(LPWSTR lpString1, LPCWSTR lpString2);
int WINAPI lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2);
void WINAPI ExitProcess(UINT uExitCode);

#define ERROR_SUCCESS            0L
#define NO_ERROR                 0L
#define CP_ACP                   0
#define CP_UTF8                  65001
#define WAIT_FAILED              0xFFFFFFFF
#define INFINITE                 0xFFFFFFFF

#define INVALID_HANDLE_VALUE     ((HANDLE)(LONG_PTR)-1)
#define INVALID_FILE_SIZE        0xFFFFFFFF

#define GENERIC_READ             0x80000000
#define GENERIC_WRITE            0x40000000
#define FILE_SHARE_READ          0x00000001
#define FILE_SHARE_WRITE         0x00000002
#define OPEN_EXISTING            3
#define OPEN_ALWAYS              4
#define FILE_ATTRIBUTE_NORMAL    0x00000080
#define WC_NO_BEST_FIT_CHARS     0x00000400
#define IS_TEXT_UNICODE_SIGNATURE         0x0008
#define IS_TEXT_UNICODE_REVERSE_SIGNATURE 0x0010
#define IS_TEXT_UNICODE_ODD_LENGTH        0x0200

#define KEY_READ                 0x20019
#define KEY_WRITE                0x20006
#define KEY_ALL_ACCESS           0xF003F
#define REG_OPTION_NON_VOLATILE  0x00000000
#define REG_SZ                   1
#define REG_BINARY               3
#define REG_DWORD                4

#define HKEY_CURRENT_USER        ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE       ((HKEY)(ULONG_PTR)0x80000002)

#define VER_PLATFORM_WIN32_NT    2
#define LOCALE_USER_DEFAULT      0x0400
#define TIME_NOSECONDS           0x00000002

LONG WINAPI RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, DWORD samDesired, HKEY *phkResult);
LONG WINAPI RegOpenKeyW(HKEY hKey, LPCWSTR lpSubKey, HKEY *phkResult);
LONG WINAPI RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
                            DWORD dwOptions, DWORD samDesired, void *lpSecurityAttributes,
                            HKEY *phkResult, DWORD *lpdwDisposition);
LONG WINAPI RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD *lpReserved, DWORD *lpType,
                             LPBYTE lpData, DWORD *lpcbData);
LONG WINAPI RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType,
                           const BYTE *lpData, DWORD cbData);
LONG WINAPI RegCloseKey(HKEY hKey);
short WINAPI GetFileTitleW(LPCWSTR lpFile, LPWSTR lpTitle, WORD cbBuf);
int WINAPI MulDiv(int nNumber, int nNumerator, int nDenominator);
UINT WINAPI GetDpiForWindow(HWND hwnd);
void WINAPI GetLocalTime(LPSYSTEMTIME lpSystemTime);
int WINAPI GetTimeFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, int cchTime);
int WINAPI GetDateFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME *lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, int cchDate);
BOOL WINAPI IsTextUnicode(const void *buf, int len, int *flags);
WORD WINAPI RtlUshortByteSwap(WORD s);

#endif
