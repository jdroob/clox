#include "common.h"
#include "memory.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (!newSize) {
        #ifdef JRMALLOC
        jrfree(pointer);
        #else
        free(pointer);
        #endif
        return NULL;
    }

    #ifdef JRMALLOC
    void *result = jrrealloc(pointer, newSize);
    #else
    void *result = realloc(pointer, newSize);
    #endif
    if (!result) {
        perror("Unable to grow array.");
        exit(EXIT_FAILURE);
    }
    return result;
}

static void freeObject(Obj_t *object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString_t *string = (ObjString_t *)object;
            FREE(ObjString_t, object);
            break;
        }
    }
}

void freeObjects(void) {
    Obj_t *next;
    Obj_t *object = vm.objects;
    while (object) {
        next = object->next;
        freeObject(object);
        object = next;
    }
}
