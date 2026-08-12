#ifndef DISCOUNT_SECURITY_H
#define DISCOUNT_SECURITY_H

#include "windows.h"

typedef enum _EXTENDED_NAME_FORMAT {
    NameUnknown = 0,
    NameFullyQualifiedDN = 1,
    NameSamCompatible = 2,
    NameDisplay = 3,
    NameUniqueId = 6,
    NameCanonical = 7,
    NameUserPrincipal = 8
} EXTENDED_NAME_FORMAT;

BOOL WINAPI GetUserNameExW(EXTENDED_NAME_FORMAT format, LPWSTR name, PULONG size);

#endif
