#ifndef DISCOUNT_ACLAPI_H
#define DISCOUNT_ACLAPI_H
#include <windows.h>
typedef void *PSID; typedef void *PSECURITY_DESCRIPTOR; typedef uint32_t ACCESS_MASK;
typedef uint32_t SECURITY_INFORMATION;
typedef struct { DWORD dwFlags; HINSTANCE hInstance; LPCWSTR pszServerName; LPCWSTR pszObjectName; LPCWSTR pszPageTitle; } SI_OBJECT_INFO;
typedef struct { GUID *pguidObjectType; ACCESS_MASK Mask; LPCWSTR pszName; DWORD dwFlags; } SI_ACCESS;
typedef struct { GUID *pguidObjectType; DWORD dwFlags; LPCWSTR pszName; } SI_INHERIT_TYPE;
typedef SI_OBJECT_INFO *PSI_OBJECT_INFO;
typedef SI_ACCESS *PSI_ACCESS;
typedef SI_INHERIT_TYPE *PSI_INHERIT_TYPE;
typedef uint32_t SI_PAGE_TYPE; typedef void *POBJECT_TYPE_LIST; typedef ACCESS_MASK *PACCESS_MASK;
typedef struct _INHERITED_FROM { LONG GenerationGap; LPWSTR AncestorName; } INHERITED_FROM;
typedef INHERITED_FROM *PINHERITED_FROM;
typedef struct { DWORD TrusteeForm; DWORD TrusteeType; union { LPWSTR pWStrName; PSID pSid; } ptstrName; } TRUSTEE_W;
typedef TRUSTEE_W TRUSTEE;
typedef struct _OBJECT_TYPE_LIST { WORD Level; WORD Sbz; GUID *ObjectType; ACCESS_MASK Mask; } OBJECT_TYPE_LIST;
typedef struct _GENERIC_MAPPING { ACCESS_MASK GenericRead; ACCESS_MASK GenericWrite; ACCESS_MASK GenericExecute; ACCESS_MASK GenericAll; } GENERIC_MAPPING;
typedef struct _ACL { BYTE AclRevision; BYTE Sbz1; WORD AclSize; WORD AceCount; WORD Sbz2; } ACL;
typedef ACL *PACL;
#define TRUSTEE_IS_SID 0
#define TRUSTEE_IS_USER 1
#define DACL_SECURITY_INFORMATION 0x00000004
#define OWNER_SECURITY_INFORMATION 0x00000001
#define GROUP_SECURITY_INFORMATION 0x00000002
#define SI_EDIT_PERMS 0x00000000
#define SI_EDIT_OWNER 0x00000001
#define SI_EDIT_AUDITS 0x00000002
#define SI_CONTAINER 0x00000004
#define SI_ADVANCED 0x00000010
#define SI_OWNER_RECURSE 0x00000020
#define SI_RESET_DACL_TREE 0x00000040
#define SI_RESET_SACL_TREE 0x00000080
#define SI_ACCESS_GENERAL 0
#define SI_ACCESS_SPECIFIC 1
#define CONTAINER_INHERIT_ACE 0x00000002
#endif
