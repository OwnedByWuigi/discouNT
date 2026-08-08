// kernel/object.c
#include <stdint.h>
#include <stddef.h>
#include "object.h"
#include "hal.h"

// Forward declarations from util.c (since we're including it)
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *d, const void *s, size_t n);
extern size_t strlen(const char *s);
extern int strcmp(const char *a, const char *b);

OBJECT_DIRECTORY *ObpRootDirectory = 0;

static void *allocate(uint32_t size) {
    static uint8_t heap[0x10000];
    static uint32_t heap_ptr = 0;
    if (heap_ptr + size > sizeof(heap)) return 0;
    void *p = &heap[heap_ptr];
    heap_ptr += size;
    return p;
}

void ObInit(void) {
    ObpRootDirectory = allocate(sizeof(OBJECT_DIRECTORY));
    memset(ObpRootDirectory, 0, sizeof(OBJECT_DIRECTORY));
    ObpRootDirectory->Header.Type = OBJ_TYPE_DIRECTORY;
    memcpy(ObpRootDirectory->Header.Name, "\\", 2);
    ObpRootDirectory->Header.RefCount = 1;
    HalDisplayString("[Ob] Object Manager initialized.\n");
}

void *ObCreateObject(uint8_t type, const char *name) {
    void *obj = 0;
    uint32_t size = 0;
    if (type == OBJ_TYPE_PROCESS) size = sizeof(PROCESS);
    else if (type == OBJ_TYPE_THREAD) size = sizeof(THREAD);
    else if (type == OBJ_TYPE_DIRECTORY) size = sizeof(OBJECT_DIRECTORY);
    else return 0;

    obj = allocate(size);
    if (!obj) return 0;
    memset(obj, 0, size);
    OBJECT_HEADER *hdr = (OBJECT_HEADER*)obj;
    hdr->Type = type;
    hdr->RefCount = 1;
    size_t len = strlen(name);
    if (len > 31) len = 31;
    memcpy(hdr->Name, name, len);
    hdr->Name[len] = 0;
    return obj;
}

void *ObOpenObjectByName(const char *name) {
    for (uint32_t i = 0; i < ObpRootDirectory->Count; i++) {
        if (!strcmp(ObpRootDirectory->Entries[i].Name, name))
            return ObpRootDirectory->Entries[i].Object;
    }
    return 0;
}