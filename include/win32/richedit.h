#ifndef DISCOUNT_RICHEDIT_H
#define DISCOUNT_RICHEDIT_H

#include "winuser.h"

#define RICHEDIT_CLASS20A "RichEdit20A"
#define SF_RTF             0x0002
#define ENM_MOUSEEVENTS    0x00020000
#define ENM_REQUESTRESIZE  0x00040000
#define EM_SETBKGNDCOLOR   (WM_USER + 67)
#define EM_SETTARGETDEVICE (WM_USER + 72)
#define EM_STREAMIN        (WM_USER + 73)
#define EM_POSFROMCHAR     (WM_USER + 38)
#define EM_SETSCROLLPOS    (WM_USER + 49)
#define EM_CHARFROMPOS     (WM_USER + 39)
#define EM_SETEVENTMASK    (WM_USER + 69)
#define EM_GETEVENTMASK    (WM_USER + 59)
#define EM_REQUESTRESIZE   (WM_USER + 65)

typedef DWORD (CALLBACK *EDITSTREAMCALLBACK)(DWORD_PTR cookie, BYTE *buffer,
                                               LONG size, LONG *written);
typedef struct _EDITSTREAM {
    DWORD_PTR dwCookie;
    DWORD dwError;
    EDITSTREAMCALLBACK pfnCallback;
} EDITSTREAM;
typedef struct _MSGFILTER { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; } MSGFILTER;
typedef struct _REQRESIZE { NMHDR nmhdr; RECT rc; } REQRESIZE;
#define EN_MSGFILTER       0x0700
#define EN_REQUESTRESIZE   0x0701

#endif
