#include <stdint.h>

typedef uint32_t DWORD;
typedef int32_t BOOL;
typedef void *PTR;

#define WINAPI __attribute__((stdcall))
#define EXPORT __attribute__((visibility("default")))

extern void CsrssGinaShowLogon(void);

static BOOL WINAPI msgina_false(void) { return 0; }
static BOOL WINAPI msgina_true(void) { return 1; }

EXPORT int DllMain(void *module, DWORD reason, void *reserved) {
    (void)module; (void)reason; (void)reserved;
    return 1;
}

EXPORT BOOL WINAPI WlxNegotiate(DWORD version, DWORD *dll_version) {
    if (!dll_version || version < 0x00010003U) return 0;
    *dll_version = 0x00010003U;
    return 1;
}

EXPORT BOOL WINAPI WlxInitialize(void *station, void *dispatch, void *context,
                                  void *options, void **handle) {
    (void)station; (void)dispatch; (void)context; (void)options;
    if (handle) *handle = 0;
    return 1;
}

EXPORT BOOL WINAPI WlxIsLockOk(void *context) { (void)context; return 1; }
EXPORT BOOL WINAPI WlxIsLogoffOk(void *context) { (void)context; return 1; }
EXPORT void WINAPI WlxDisconnectNotify(void *context) { (void)context; }
EXPORT void WINAPI WlxDisplayLockedNotice(void *context) { (void)context; }
EXPORT void WINAPI WlxDisplaySASNotice(void *context) { (void)context; }
EXPORT BOOL WINAPI WlxRemoveStatusMessage(void *context) { (void)context; return 1; }
EXPORT BOOL WINAPI WlxGetStatusMessage(void *context, DWORD *options,
                                       void *message, DWORD length) {
    (void)context; (void)options; (void)message; (void)length; return 0;
}
EXPORT BOOL WINAPI WlxNetworkProviderLoad(void *context, void *notify) {
    (void)context; (void)notify; return 0;
}
EXPORT BOOL WINAPI WlxGetConsoleSwitchCredentials(void *context, void *info) {
    (void)context; (void)info; return 0;
}
EXPORT BOOL WINAPI WlxScreenSaverNotify(void *context, void *notify) {
    (void)context; (void)notify; return 1;
}
EXPORT BOOL WINAPI WlxStartApplication(void *context, void *desktop,
                                       void *environment, void *command) {
    (void)context; (void)desktop; (void)environment; (void)command; return 0;
}
EXPORT BOOL WINAPI WlxActivateUserShell(void *context, void *desktop,
                                        void *environment, void *command) {
    (void)context; (void)desktop; (void)environment; (void)command; return 1;
}
EXPORT int WINAPI WlxLoggedOnSAS(void *context, DWORD sas, void *info) {
    (void)context; (void)sas; (void)info; return 0;
}
EXPORT int WINAPI WlxLoggedOutSAS(void *context, DWORD sas, void *info,
                                  void *user, void *domain, void *password,
                                  void *profile) {
    (void)context; (void)sas; (void)info; (void)user; (void)domain;
    (void)password; (void)profile;
    CsrssGinaShowLogon();
    return 0;
}
EXPORT int WINAPI WlxWkstaLockedSAS(void *context, DWORD sas) {
    (void)context; (void)sas; return 0;
}
EXPORT BOOL WINAPI WlxLogoff(void *context) { (void)context; return 1; }
EXPORT BOOL WINAPI WlxShutdown(void *context, DWORD action) {
    (void)context; (void)action; return 1;
}

EXPORT BOOL WINAPI ShellIsFriendlyUIActive(void) { return 0; }
EXPORT BOOL WINAPI ShellTurnOffDialog(void *window) { (void)window; return 1; }
EXPORT BOOL WINAPI ShellShutdownDialog(void *window, void *caption, DWORD flags) {
    (void)window; (void)caption; (void)flags; return 1;
}
EXPORT BOOL WINAPI ShellDimScreen(void *window, void *context) {
    (void)window; (void)context; return 1;
}

/* ReactOS exports these helpers from the same module. They are deliberately
 * harmless until Winlogon/session management is implemented in CSRSS. */
EXPORT BOOL WINAPI ShellGetUserList(uint32_t a, uint32_t b, uint32_t c) { (void)a;(void)b;(void)c;return 0; }
EXPORT BOOL WINAPI ShellStatusHostEnd(uint32_t a) { (void)a; return 1; }
EXPORT BOOL WINAPI ShellIsSuspendAllowed(void) { return 0; }
EXPORT BOOL WINAPI ShellIsRemoteConnectionsEnabled(void) { return 0; }
EXPORT BOOL WINAPI ShellEnableFriendlyUI(uint32_t a) { (void)a; return 1; }
EXPORT BOOL WINAPI ShellEnableMultipleUsers(uint32_t a) { (void)a; return 0; }
EXPORT BOOL WINAPI ShellEnableRemoteConnections(uint32_t a) { (void)a; return 0; }
EXPORT BOOL WINAPI ShellIsMultipleUsersEnabled(void) { return 0; }
EXPORT BOOL WINAPI ShellACPIPowerButtonPressed(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;return 0;}
EXPORT BOOL WINAPI ShellIsSingleUserNoPassword(void *a,void *b){(void)a;(void)b;return 0;}
EXPORT BOOL WINAPI ShellStatusHostShuttingDown(void){return 1;}
EXPORT BOOL WINAPI ShellNotifyThemeUserChange(uint32_t a,uint32_t b){(void)a;(void)b;return 1;}
EXPORT BOOL WINAPI ShellSwitchWhenInteractiveReady(uint32_t a,uint32_t b){(void)a;(void)b;return 0;}
EXPORT BOOL WINAPI ShellInstallAccountFilterData(void){return 1;}
EXPORT BOOL WINAPI ShellStatusHostBegin(uint32_t a){(void)a;return 1;}
EXPORT BOOL WINAPI ShellIsUserInteractiveLogonAllowed(uint32_t a){(void)a;return 1;}
EXPORT BOOL WINAPI ShellSwitchUser(uint32_t a){(void)a;return 0;}
EXPORT BOOL WINAPI ShellReturnToWelcome(uint32_t a){(void)a;return 0;}
EXPORT BOOL WINAPI ShellStatusHostPowerEvent(void){return 1;}
EXPORT BOOL WINAPI ShellStartCredentialServer(void *a,uint32_t b,uint32_t c,uint32_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
EXPORT BOOL WINAPI ShellAcquireLogonMutex(void){return 1;}
EXPORT BOOL WINAPI ShellReleaseLogonMutex(uint32_t a){(void)a;return 1;}
EXPORT BOOL WINAPI ShellSignalShutdown(void){return 1;}
EXPORT BOOL WINAPI ShellStatusHostHide(void){return 1;}
EXPORT BOOL WINAPI ShellStatusHostShow(void){return 1;}
EXPORT BOOL WINAPI WlxReconnectNotify(uint32_t a){(void)a;return 0;}
