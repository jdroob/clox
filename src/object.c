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
    obj->isMarked = false;
    obj->isProtected = true;
    vm.objects = obj;
    #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void *)obj, size, objectType);
    #endif
    return obj;
}        

static ObjString_t *allocateString(char *chars, int length, uint32_t hash) {
    ObjString_t *string = ALLOCATE_OBJ(ObjString_t, sizeof(ObjString_t) + length + 1, OBJ_STRING);
    string->length = length;
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

ObjClosure_t *newClosure(ObjFunction_t *function) {
    ObjClosure_t *closure = ALLOCATE_OBJ(ObjClosure_t, sizeof(ObjClosure_t), OBJ_CLOSURE);
    closure->function = function;
    
    ObjUpvalue_t **upvalues = ALLOCATE(ObjUpvalue_t*, function->upvalueCount);
    for (int i=0; i<function->upvalueCount; ++i) {
        upvalues[i] = NULL;
    }

    closure->upvalueCount = function->upvalueCount;
    closure->upvalues = upvalues;
    return closure;
}

ObjFunction_t *newFunction(void) {
    ObjFunction_t *function = ALLOCATE_OBJ(ObjFunction_t, sizeof(ObjFunction_t), OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    function->upvalueCount = 0;
    function->upvalueCapacity = 0;
    return function;
}

ObjNative_t *newNative(NativeFn_t function, int arity) {
    ObjNative_t *native = ALLOCATE_OBJ(ObjNative_t, sizeof(ObjNative_t), OBJ_FUNCTION);
    native->function = function;
    native->arity = arity;
    native->obj.type = OBJ_NATIVE;
    return native;
}

ObjFileHandle_t *newFileHandle(FILE *fh, const char *name, const char *accessType) {
    ObjFileHandle_t *fileHandle = ALLOCATE_OBJ(ObjFileHandle_t, sizeof(ObjFileHandle_t), OBJ_FILEHANDLE);
    fileHandle->fh = fh;
    fileHandle->obj.type = OBJ_FILEHANDLE;
    fileHandle->name = name;
    fileHandle->accessType = accessType;
    fileHandle->isOpen = true;
    return fileHandle;
}

ObjUpvalue_t *newUpvalue(Value_t *slot) {
    ObjUpvalue_t *upvalue = ALLOCATE_OBJ(ObjUpvalue_t, sizeof(ObjUpvalue_t), OBJ_UPVALUE);
    upvalue->obj.type = OBJ_UPVALUE;
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    return upvalue;
}

ObjClass_t *newClass(ObjString_t *name) {
    ObjClass_t *klass = ALLOCATE_OBJ(ObjClass_t, sizeof(ObjClass_t), OBJ_CLASS);
    klass->obj.type = OBJ_CLASS;
    klass->methods = NULL;
    klass->name = name;
}

ObjString_t *makeString(char *chars, int length) {
    // NOTE: makeString expects length == strlen(s)
    //       +1 for null byte accounted for in allocateString
    uint32_t hash = hashString(chars, length);
    ObjString_t *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    return allocateString(chars, length, hash);
}

void turnOnProtectMode(Obj_t *object) {
    object->isProtected = true;
}

void turnOffProtectMode(Obj_t *object) {
    object->isProtected = false;
}

static void printFunction(ObjFunction_t *function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %.*s>", function->name->length, function->name->chars);
}

void printObject(Value_t val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_CLASS: {
            printf("<class: %s>", AS_CLASS(val)->name->chars);
            if (appendNewline) printf("\n");
            break;
        }
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
            if (appendNewline) printf("\n");
            break;
        }
        case OBJ_FILEHANDLE: {
            printf("<file handle: %s>", AS_FILEHANDLE(val)->name);
            if (appendNewline) printf("\n");
            break;
        }
        case OBJ_CLOSURE: {
            printFunction(AS_CLOSURE(val)->function);
            if (appendNewline) printf("\n");
            break;
        }
        case OBJ_UPVALUE: {
            printf("<upvalue %p>", AS_UPVALUE(val)->location);
            if (appendNewline) printf("\n");
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
