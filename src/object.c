#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
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
    string->isConst = isConst;
    string->hash = hash;
    memcpy(string->chars, chars, string->length);
    string->chars[length] = '\0';
    tableSet(&vm.strings, string, NIL_VAL);
    return string;
}

static uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;
    for (int i=0; i<length; ++i) {
        hash ^= (uint8_t)key[i];
        hash *= 1677719;
    }
    return hash;
}

ObjString_t *makeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString_t *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    return allocateString(chars, length, false, hash);
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
