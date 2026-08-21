#ifndef DISCOUNT_NTUSER_H
#define DISCOUNT_NTUSER_H
#include "windows.h"
enum { NtUserClipboardWindowProc=0x0300,NtUserSystemTrayCall=0x0306 };
enum wine_systray_call { WINE_SYSTRAY_NOTIFY_ICON,WINE_SYSTRAY_CLEANUP_ICONS,WINE_SYSTRAY_DOCK_INIT,WINE_SYSTRAY_DOCK_INSERT,WINE_SYSTRAY_DOCK_CLEAR,WINE_SYSTRAY_DOCK_REMOVE };
LRESULT WINAPI NtUserMessageCall(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam,void *result,UINT type,BOOL ansi);
#endif
