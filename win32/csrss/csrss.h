#ifndef WIN32_CSRSS_H
#define WIN32_CSRSS_H

void CsrssSessionRun(void *mb_info);
int CsrssInitialize(void *boot_info);
int CsrssIsInitialized(void);

#endif
