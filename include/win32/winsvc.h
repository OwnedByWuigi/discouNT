#ifndef DISCOUNT_WINSVC_H
#define DISCOUNT_WINSVC_H

#include "windows.h"

typedef HANDLE SC_HANDLE;

typedef struct _SERVICE_STATUS {
    DWORD dwServiceType;
    DWORD dwCurrentState;
    DWORD dwControlsAccepted;
    DWORD dwWin32ExitCode;
    DWORD dwServiceSpecificExitCode;
    DWORD dwCheckPoint;
    DWORD dwWaitHint;
} SERVICE_STATUS, *LPSERVICE_STATUS;

typedef struct _SERVICE_DESCRIPTIONW { LPWSTR lpDescription; } SERVICE_DESCRIPTIONW;
typedef struct _SC_ACTION { DWORD Type; DWORD Delay; } SC_ACTION;
typedef struct _SERVICE_FAILURE_ACTIONSW {
    DWORD dwResetPeriod;
    LPWSTR lpRebootMsg;
    LPWSTR lpCommand;
    DWORD cActions;
    SC_ACTION *lpsaActions;
} SERVICE_FAILURE_ACTIONSW;

#define SERVICE_KERNEL_DRIVER        0x00000001
#define SERVICE_FILE_SYSTEM_DRIVER   0x00000002
#define SERVICE_RECOGNIZER_DRIVER    0x00000008
#define SERVICE_WIN32_OWN_PROCESS    0x00000010
#define SERVICE_WIN32_SHARE_PROCESS  0x00000020
#define SERVICE_WIN32                0x00000030
#define SERVICE_INTERACTIVE_PROCESS  0x00000100

#define SERVICE_BOOT_START           0x00000000
#define SERVICE_SYSTEM_START         0x00000001
#define SERVICE_AUTO_START           0x00000002
#define SERVICE_DEMAND_START         0x00000003
#define SERVICE_DISABLED             0x00000004

#define SERVICE_STOPPED              0x00000001
#define SERVICE_START_PENDING        0x00000002
#define SERVICE_STOP_PENDING         0x00000003
#define SERVICE_RUNNING              0x00000004
#define SERVICE_CONTINUE_PENDING     0x00000005
#define SERVICE_PAUSE_PENDING        0x00000006
#define SERVICE_PAUSED               0x00000007

#define SERVICE_ALL_ACCESS           0x000F01FF
#define SERVICE_START                0x00000010
#define SERVICE_STOP                 0x00000020
#define SERVICE_QUERY_STATUS         0x00000004
#define SERVICE_CHANGE_CONFIG       0x00000002
#define SC_MANAGER_ALL_ACCESS        0x000F003F
#define DELETE                       0x00010000

#define SERVICE_CONFIG_DESCRIPTION   1
#define SERVICE_CONFIG_FAILURE_ACTIONS 2
#define SERVICE_CONTROL_STOP          1
#define SERVICE_ERROR_NORMAL         1
#define SERVICE_ERROR_SEVERE         2
#define SERVICE_ERROR_CRITICAL       3
#define SERVICE_ERROR_IGNORE         0
#define SC_ACTION_NONE               0
#define SC_ACTION_RESTART            1
#define SC_ACTION_REBOOT             2
#define SC_ACTION_RUN_COMMAND        3

SC_HANDLE WINAPI OpenSCManagerW(LPCWSTR machine, LPCWSTR database, DWORD access);
SC_HANDLE WINAPI OpenServiceW(SC_HANDLE manager, LPCWSTR name, DWORD access);
SC_HANDLE WINAPI CreateServiceW(SC_HANDLE manager, LPCWSTR name, LPCWSTR display,
                                DWORD access, DWORD type, DWORD start, DWORD error,
                                LPCWSTR binary, LPCWSTR group, DWORD *tag,
                                LPCWSTR depend, LPCWSTR object, LPCWSTR password);
BOOL WINAPI CloseServiceHandle(SC_HANDLE service);
BOOL WINAPI DeleteService(SC_HANDLE service);
BOOL WINAPI StartServiceW(SC_HANDLE service, DWORD argc, LPCWSTR *argv);
BOOL WINAPI ControlService(SC_HANDLE service, DWORD control, LPSERVICE_STATUS status);
BOOL WINAPI QueryServiceStatus(SC_HANDLE service, LPSERVICE_STATUS status);
BOOL WINAPI ChangeServiceConfig2W(SC_HANDLE service, DWORD level, void *info);

#endif
