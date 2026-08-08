// kernel/object.h
#ifndef OBJECT_H
#define OBJECT_H
#include <stdint.h>

#define OBJ_TYPE_PROCESS  1
#define OBJ_TYPE_THREAD   2
#define OBJ_TYPE_DIRECTORY 3

typedef struct _OBJECT_HEADER {
    uint8_t  Type;
    char     Name[32];
    uint32_t RefCount;
} OBJECT_HEADER;

typedef struct _OBJECT_DIRECTORY_ENTRY {
    char             Name[32];
    struct _OBJECT_HEADER *Object;
} OBJECT_DIRECTORY_ENTRY;

typedef struct _OBJECT_DIRECTORY {
    OBJECT_HEADER Header;
    uint32_t      Count;
    OBJECT_DIRECTORY_ENTRY Entries[64];
} OBJECT_DIRECTORY;

typedef struct _PROCESS {
    OBJECT_HEADER Header;
    void          *HandleTable[32];
} PROCESS;

typedef struct _THREAD {
    OBJECT_HEADER Header;
    void          *Process;
    uint32_t      KernelStack;   // saved esp
    uint32_t      State;         // 0=ready, 1=running
} THREAD;

// Global root directory
extern OBJECT_DIRECTORY *ObpRootDirectory;

void ObInit(void);
void *ObCreateObject(uint8_t type, const char *name);
void *ObOpenObjectByName(const char *name);
#endif