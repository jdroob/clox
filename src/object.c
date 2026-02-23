#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "debug.h"
#include "vm.h"


#define ALLOCATE_OBJ(type, length, objectType) \
        (type *)allocateObject(length, objectType)

static Obj_t *allocateObject(size_t size, Obj_e objectType) {
    Obj_t *obj = (Obj_t *)reallocate(NULL, 0, size);
    obj->type = objectType;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}        

static ObjString_t *allocateString(char *chars, int length, bool isConst, uint32_t hash) {
    ObjString_t *string = ALLOCATE_OBJ(ObjString_t, sizeof(ObjString_t) + length + 1, OBJ_STRING);
    string->length = length;
    string->isConst = isConst;  // TODO: remove isConst - no longer needed
    string->hash = hash;
    memcpy(string->chars, chars, string->length);
    string->chars[length] = '\0';
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
    return string;
}

static uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;    // 0x811C_9DC5
    for (int i=0; i<length; ++i) {
        hash ^= (uint8_t)key[i];
        hash *= 1677719;            // 0x0000_418B
    }
    return hash;
}

ObjFunction_t *newFunction(void) {
    ObjFunction_t *function = ALLOCATE_OBJ(ObjFunction_t, sizeof(ObjFunction_t), OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative_t *newNative(NativeFn_t function) {
    ObjNative_t *native = ALLOCATE_OBJ(ObjNative_t, sizeof(ObjNative_t), OBJ_FUNCTION);
    native->function = function;
    native->obj.type = OBJ_NATIVE;
    return native;
}

ObjString_t *makeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString_t *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    return allocateString(chars, length, false, hash);
}

static void printFunction(ObjFunction_t *function) {
    if (function->name == NULL) {
    printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void printObject(Value_t val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING: {
            printf("\"%s\"", AS_CSTRING(val));
            if (appendNewline) printf("\n");
            break;
        }
        case OBJ_FUNCTION: {
            printFunction(AS_FUNCTION(val));
            if (appendNewline) printf("\n");
            break;
        }
        case OBJ_NATIVE: {
            printf("<native fn>");
            break;
        }
    }
}

// ObjString_t *takeString(char *chars, int length) {
//     uint32_t hash = hashString(chars, length);
//     ObjString_t *interned = tableFindString(&vm.strings, chars, length, hash);
//     if (interned != NULL) return interned;
//     return allocateString(chars, length, false, hash);
// }

// ObjString_t *copyString(const char *chars, int length) {
//     uint32_t hash = hashString(chars, length);
//     ObjString_t *interned = tableFindString(&vm.strings, chars, length, hash);
//     if (interned != NULL) return interned;
//     return allocateString((char *)chars, length, true, hash);
// }
