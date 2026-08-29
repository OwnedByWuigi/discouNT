#ifndef KEXPORTS_H
#define KEXPORTS_H

void *KernelResolveSymbol(const char *name);
void *KernelResolveExport(const char *name);

#endif
