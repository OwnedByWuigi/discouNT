#ifndef DISCOUNT_MSGINA_NTSECAPI_H
#define DISCOUNT_MSGINA_NTSECAPI_H
#include "winnt.h"
typedef HANDLE LSA_HANDLE;
typedef struct _LSA_OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} LSA_OBJECT_ATTRIBUTES, *PLSA_OBJECT_ATTRIBUTES;
typedef UNICODE_STRING LSA_UNICODE_STRING, *PLSA_UNICODE_STRING;
typedef NTSTATUS *PLSA_STATUS;
typedef ULONG LSA_OPERATIONAL_MODE;
typedef ANSI_STRING LSA_STRING, *PLSA_STRING;
#define MSV1_0_CHANGEPASSWORD 3
#define MsV1_0ChangePassword 3
typedef struct _MSV1_0_CHANGEPASSWORD_REQUEST {
    ULONG MessageType;
    BOOLEAN Impersonating;
    UNICODE_STRING DomainName;
    UNICODE_STRING AccountName;
    UNICODE_STRING OldPassword;
    UNICODE_STRING NewPassword;
} MSV1_0_CHANGEPASSWORD_REQUEST, *PMSV1_0_CHANGEPASSWORD_REQUEST;
typedef struct _MSV1_0_CHANGEPASSWORD_RESPONSE { ULONG MessageType; NTSTATUS AccountStoreStatus; } MSV1_0_CHANGEPASSWORD_RESPONSE, *PMSV1_0_CHANGEPASSWORD_RESPONSE;
#define POLICY_GET_PRIVATE_INFORMATION 0x00000004
NTSTATUS WINAPI LsaOpenPolicy(PLSA_UNICODE_STRING name, PLSA_OBJECT_ATTRIBUTES attrs,
                              ACCESS_MASK access, LSA_HANDLE *handle);
NTSTATUS WINAPI LsaRetrievePrivateData(LSA_HANDLE handle, PLSA_UNICODE_STRING key,
                                        PLSA_UNICODE_STRING *data);
NTSTATUS WINAPI LsaFreeMemory(PVOID data);
NTSTATUS WINAPI LsaClose(LSA_HANDLE handle);
NTSTATUS WINAPI LsaDeregisterLogonProcess(LSA_HANDLE handle);
NTSTATUS WINAPI LsaCallAuthenticationPackage(LSA_HANDLE handle, ULONG package, PVOID request,
                                              ULONG request_len, PVOID *response, PULONG response_len,
                                              PNTSTATUS protocol_status);
NTSTATUS WINAPI LsaFreeReturnBuffer(PVOID buffer);
#endif
