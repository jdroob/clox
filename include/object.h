#ifndef CLOX_OBJ_H
#define CLOX_OBJ_H

#include "common.h"
#include "value.h"

typedef enum {
    OBJ_STRING
} Obj_e;

struct Obj_t {
    Obj_e type;
    Obj_t *next;
};

struct ObjString_t {
    Obj_t obj;
    int length;
    bool isConst;
    char chars[];
};

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)
#define IS_STRING(value)  isObjType(value, OBJ_STRING)
#define AS_STRING(value)  ((ObjString_t *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString_t *)AS_OBJ(value))->chars)

static inline bool isObjType(Value_t value, Obj_e type) {
    return (IS_OBJ(value) && OBJ_TYPE(value) == type);
}

ObjString_t *takeString(char *chars, int length);   // create dynamic string
ObjString_t *copyString(const char *chars, int length); // copy string from source

# endif // CLOX_OBJ_H