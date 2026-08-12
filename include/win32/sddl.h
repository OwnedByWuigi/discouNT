#ifndef DISCOUNT_SDDL_H
#define DISCOUNT_SDDL_H

#include "windows.h"

BOOL WINAPI ConvertSidToStringSidW(PSID sid, LPWSTR *string_sid);
DWORD WINAPI GetLengthSid(PSID sid);
BOOL WINAPI CopySid(DWORD length, PSID destination, PSID source);

#endif
