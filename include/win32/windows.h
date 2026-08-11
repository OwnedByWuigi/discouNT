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
#define WAIT_FAILED              0xFFFFFFFF
#define INFINITE                 0xFFFFFFFF

#define KEY_READ                 0x20019
#define KEY_WRITE                0x20006
#define REG_OPTION_NON_VOLATILE  0x00000000
#define REG_BINARY               3

#define HKEY_CURRENT_USER        ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE       ((HKEY)(ULONG_PTR)0x80000002)

#define VER_PLATFORM_WIN32_NT    2

LONG WINAPI RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, DWORD samDesired, HKEY *phkResult);
LONG WINAPI RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
                            DWORD dwOptions, DWORD samDesired, void *lpSecurityAttributes,
                            HKEY *phkResult, DWORD *lpdwDisposition);
LONG WINAPI RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD *lpReserved, DWORD *lpType,
                             LPBYTE lpData, DWORD *lpcbData);
LONG WINAPI RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType,
                           const BYTE *lpData, DWORD cbData);
LONG WINAPI RegCloseKey(HKEY hKey);

#endif
