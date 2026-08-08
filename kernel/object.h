#ifndef OBJECT_H
#define OBJECT_H
#include <stdint.h>

#define MAX_OBJECTS 64
#define MAX_NAME_LEN 32

typedef enum {
    OBJ_TYPE_NONE = 0,
    OBJ_TYPE_PROCESS,
    OBJ_TYPE_THREAD,
    OBJ_TYPE_EVENT,
    OBJ_TYPE_MUTEX,
    OBJ_TYPE_WINDOW,    // Win32 window
    OBJ_TYPE_PEN,       // GDI pen
    OBJ_TYPE_BRUSH,     // GDI brush
    OBJ_TYPE_DC,        // Device context
} OBJECT_TYPE;

typedef struct _OBJECT {
    char     name[MAX_NAME_LEN];
    OBJECT_TYPE type;
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
HANDLE ObCreateObject(OBJECT_TYPE type, const char *name, void *body, uint32_t body_size);
void *ObReferenceObject(HANDLE handle);
void ObDereferenceObject(HANDLE handle);
HANDLE ObFindObject(const char *name, OBJECT_TYPE type);
#endif