#ifndef DISCOUNT_MSGINA_WINDOWSX_H
#define DISCOUNT_MSGINA_WINDOWSX_H
#include "windows.h"
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define HANDLE_WM_COMMAND(hwnd,wParam,lParam,fn) ((fn)((hwnd),(int)LOWORD(wParam),(HWND)(lParam),HIWORD(wParam)))
#endif
