#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, length, objectType) \
        (type *)allocateObject(sizeof(type) + length, objectType)

static Obj_t *allocateObject(size_t size, Obj_e objectType) {
    Obj_t *obj = (Obj_t *)reallocate(NULL, 0, size);
    obj->type = objectType;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}        

static ObjString_t *allocateString(char *chars, int length) {
    ObjString_t *string = ALLOCATE_OBJ(ObjString_t, length + 1, OBJ_STRING);
    string->length = length;
    memcpy(string->chars, chars, string->length);
    string->chars[length] = '\0';
    return string;
}

ObjString_t *takeString(char *chars, int length) {
    return allocateString(chars, length);
}

ObjString_t *copyString(const char *chars, int length) {
    // char *heapChars = ALLOCATE(char, length + 1);
    // memcpy(heapChars, chars, length);
    // heapChars[length] = '\0';
    return allocateString(chars, length);
}
