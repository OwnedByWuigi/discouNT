#ifndef DISCOUNT_MSGINA_USERENV_H
#define DISCOUNT_MSGINA_USERENV_H
#include "windows.h"
BOOL WINAPI CreateEnvironmentBlock(LPVOID *lpEnvironment, HANDLE hToken, BOOL inherit);
BOOL WINAPI DestroyEnvironmentBlock(LPVOID lpEnvironment);
BOOL WINAPI LoadUserProfileW(HANDLE token, LPVOID profile);
#endif
