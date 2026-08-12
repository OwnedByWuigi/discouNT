#ifndef DISCOUNT_MSGINA_WINWLX_H
#define DISCOUNT_MSGINA_WINWLX_H
#include "windows.h"
#define WLX_VERSION_1_3 0x00010003
#define WLX_SAS_TYPE_CTRL_ALT_DEL 1
#define WLX_SAS_TYPE_TIMEOUT 2
#define WLX_SAS_TYPE_SC_INSERT 3
#define WLX_SAS_TYPE_SC_REMOVE 4
#define WLX_SAS_ACTION_NONE 0
#define WLX_SAS_ACTION_LOGON 1
#define WLX_SAS_ACTION_LOCK_WKSTA 2
#define WLX_SAS_ACTION_LOGOFF 3
#define WLX_SAS_ACTION_SHUTDOWN 5
#define WLX_SAS_ACTION_PWD_CHANGED 6
#define WLX_SAS_ACTION_TASKLIST 7
#define WLX_SAS_ACTION_UNLOCK_WKSTA 8
#define WLX_SAS_ACTION_SWITCH_CONSOLE 9
#define WLX_SAS_ACTION_FORCE_LOGOFF 10
#define WLX_SAS_ACTION_SHUTDOWN_POWER_OFF 5
#define WLX_PROFILE_TYPE_V2_0 2
typedef struct _WLX_PROFILE_V2_0 {
    DWORD dwType;
    LPWSTR pszProfile;
    LPWSTR pszPolicy;
    LPWSTR pszNetworkDefaultUserProfile;
    LPWSTR pszServerName;
    LPWSTR pszEnvironment;
} WLX_PROFILE_V2_0, *PWLX_PROFILE_V2_0;
typedef struct _WLX_MPR_NOTIFY_INFO {
    LPWSTR pszUserName;
    LPWSTR pszDomain;
    LPWSTR pszPassword;
    LPWSTR pszOldPassword;
} WLX_MPR_NOTIFY_INFO, *PWLX_MPR_NOTIFY_INFO;
typedef struct _WLX_DISPATCH_VERSION_1_3 {
    BOOL (WINAPI *WlxUseCtrlAltDel)(HANDLE);
    void *WlxSetContextPointer;
    VOID (WINAPI *WlxSasNotify)(HANDLE, DWORD);
    void *WlxSetTimeout;
    void *WlxAssignShellProtection;
    INT_PTR (WINAPI *WlxMessageBox)(HANDLE, HWND, LPCWSTR, LPCWSTR, UINT);
    void *WlxDialogBox;
    INT_PTR (WINAPI *WlxDialogBoxParam)(HANDLE, HINSTANCE, LPCWSTR, HWND, void *, LPARAM);
    void *WlxDialogBoxIndirect;
    void *WlxDialogBoxIndirectParam;
    void *WlxSwitchDesktopToUser;
    void *WlxSwitchDesktopToWinlogon;
    void *WlxChangePasswordNotify;
    void *WlxGetSourceDesktop;
    void *WlxSetReturnDesktop;
    void *WlxCreateUserDesktop;
    void *WlxChangePasswordNotifyEx;
    void *WlxCloseUserDesktop;
    void *WlxSetOption;
    void *WlxGetOption;
    void *WlxWin31Migrate;
    void *WlxQueryClientCredentials;
    void *WlxQueryInetConnectorCredentials;
    void *WlxDisconnect;
    void *WlxQueryTerminalServicesData;
} WLX_DISPATCH_VERSION_1_3, *PWLX_DISPATCH_VERSION_1_3;
#endif
