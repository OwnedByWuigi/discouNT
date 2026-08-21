#include <stdint.h>
#include "ob/object.h"
#include "mm/mm.h"
#include "hal.h"
#include "core/util.h"
#include "serial.h"

static HANDLE_TABLE global_table;
static OBJECT_TYPE_INFO object_types[MAX_OBJECT_TYPES];
static uint32_t object_type_count;

uint32_t ObRegisterObjectType(const char *name, OBJECT_DELETE_PROC delete_proc) {
    uint32_t i;
    int len;

    if (!name || !name[0]) return 0;

    /* Registration is idempotent so independently loaded components can
     * safely request a shared type by name. */
    for (i = 1; i < object_type_count; i++) {
        if (strcmp(object_types[i].name, name) == 0)
            return i;
    }

    if (object_type_count >= MAX_OBJECT_TYPES) {
        SerialPutString("[OBJ] Object type registry is full\r\n");
        return 0;
    }

    i = object_type_count++;
    len = strlen(name);
    if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
    memcpy(object_types[i].name, name, len);
    object_types[i].name[len] = 0;
    object_types[i].delete_proc = delete_proc;
    return i;
}

const OBJECT_TYPE_INFO *ObGetObjectType(uint32_t type) {
    if (type == 0 || type >= object_type_count) return 0;
    return &object_types[type];
}

void ObInit(void) {
    memset(&global_table, 0, sizeof(HANDLE_TABLE));
    memset(object_types, 0, sizeof(object_types));
    object_type_count = 1; /* Type zero is always invalid. */

}

HANDLE ObCreateObject(uint32_t type, const char *name, void *body, uint32_t body_size) {
    if (!ObGetObjectType(type)) {
        SerialPutString("[OBJ] ObCreateObject called with unregistered type\r\n");
        return INVALID_HANDLE;
    }

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
        if (obj->body) {
            const OBJECT_TYPE_INFO *type = ObGetObjectType(obj->type);
            if (type && type->delete_proc) type->delete_proc(obj->body);
            kfree(obj->body);
        }
        kfree(obj);
        global_table.objects[handle] = 0;
        global_table.count--;
    }
}

HANDLE ObFindObject(const char *name, uint32_t type) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        OBJECT *obj = global_table.objects[i];
        if (obj && obj->type == type && name) {
            if (strcmp(obj->name, name) == 0)
                return (HANDLE)i;
        }
    }
    return INVALID_HANDLE;
}
