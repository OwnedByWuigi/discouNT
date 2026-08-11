#include <stdint.h>
#include "object.h"
#include "mm.h"
#include "hal.h"
#include "util.h"
#include "serial.h"

static HANDLE_TABLE global_table;

void ObInit(void) {
    memset(&global_table, 0, sizeof(HANDLE_TABLE));
}

HANDLE ObCreateObject(OBJECT_TYPE type, const char *name, void *body, uint32_t body_size) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (global_table.objects[i] == 0) {
            OBJECT *obj = (OBJECT*)kmalloc(sizeof(OBJECT));
            if (!obj) return INVALID_HANDLE;
            
            memset(obj, 0, sizeof(OBJECT));
            obj->type = type;
            obj->ref_count = 1;
            obj->body = body;
            obj->body_size = body_size;
            
            if (name) {
                int len = strlen(name);
                if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
                memcpy(obj->name, name, len);
                obj->name[len] = 0;
            }
            
            global_table.objects[i] = obj;
            global_table.count++;
            return (HANDLE)i;
        }
    }
    SerialPutString("[OBJ] ObCreateObject exhausted handle table\r\n");
    return INVALID_HANDLE;
}

void *ObReferenceObject(HANDLE handle) {
    if (handle >= MAX_OBJECTS || !global_table.objects[handle])
        return 0;
    
    global_table.objects[handle]->ref_count++;
    return global_table.objects[handle]->body;
}

void ObDereferenceObject(HANDLE handle) {
    if (handle >= MAX_OBJECTS || !global_table.objects[handle])
        return;
    
    OBJECT *obj = global_table.objects[handle];
    obj->ref_count--;
    
    if (obj->ref_count == 0) {
        if (obj->body) kfree(obj->body);
        kfree(obj);
        global_table.objects[handle] = 0;
        global_table.count--;
    }
}

HANDLE ObFindObject(const char *name, OBJECT_TYPE type) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        OBJECT *obj = global_table.objects[i];
        if (obj && obj->type == type && name) {
            if (strcmp(obj->name, name) == 0)
                return (HANDLE)i;
        }
    }
    return INVALID_HANDLE;
}
