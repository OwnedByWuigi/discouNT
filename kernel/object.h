#ifndef OBJECT_H
#define OBJECT_H
#include <stdint.h>

#define MAX_OBJECTS 256
#define MAX_OBJECT_TYPES 32
#define MAX_NAME_LEN 32

typedef void (*OBJECT_DELETE_PROC)(void *body);

typedef struct _OBJECT_TYPE_INFO {
    char name[MAX_NAME_LEN];
    OBJECT_DELETE_PROC delete_proc;
} OBJECT_TYPE_INFO;

typedef struct _OBJECT {
    char     name[MAX_NAME_LEN];
    uint32_t type;
    uint32_t ref_count;
    void     *body;       // Pointer to actual object data
    uint32_t body_size;
} OBJECT;

typedef struct _HANDLE_TABLE {
    OBJECT *objects[MAX_OBJECTS];
    uint32_t count;
} HANDLE_TABLE;

// Global handles
#define INVALID_HANDLE 0xFFFFFFFF
typedef uint32_t HANDLE;

void ObInit(void);
uint32_t ObRegisterObjectType(const char *name, OBJECT_DELETE_PROC delete_proc);
const OBJECT_TYPE_INFO *ObGetObjectType(uint32_t type);
HANDLE ObCreateObject(uint32_t type, const char *name, void *body, uint32_t body_size);
void *ObReferenceObject(HANDLE handle);
void ObDereferenceObject(HANDLE handle);
HANDLE ObFindObject(const char *name, uint32_t type);
#endif
