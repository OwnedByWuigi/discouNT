/* Minimal desktop bridge for the ReactOS MSGINA core.
 * The authentication/session policy remains in msgina.c; the desktop UI is
 * supplied by CSRSS, just as Winlogon supplies a desktop to a GINA. */
#include "../msgina.h"

extern void CsrssGinaShowLogon(void);

static BOOL port_initialize(PGINA_CONTEXT c) { (void)c; return TRUE; }
static BOOL port_status(PGINA_CONTEXT c, HDESK d, DWORD o, PWSTR t, PWSTR m)
{ (void)c; (void)d; (void)o; (void)t; (void)m; return TRUE; }
static BOOL port_remove(PGINA_CONTEXT c) { (void)c; return TRUE; }
static VOID port_notice(PGINA_CONTEXT c) { (void)c; }
static INT port_logged_on(PGINA_CONTEXT c, DWORD sas)
{ (void)c; (void)sas; return WLX_SAS_ACTION_NONE; }
static INT port_logged_out(PGINA_CONTEXT c)
{ (void)c; CsrssGinaShowLogon(); return WLX_SAS_ACTION_NONE; }
static INT port_locked(PGINA_CONTEXT c) { (void)c; return WLX_SAS_ACTION_NONE; }
static VOID port_locked_notice(PGINA_CONTEXT c) { (void)c; }

GINA_UI GinaTextUI = {
    port_initialize, port_status, port_remove, port_notice,
    port_logged_on, port_logged_out, port_locked, port_locked_notice
};
