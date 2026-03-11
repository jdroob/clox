#ifndef CLOX_OBJ_H
#define CLOX_OBJ_H

#include "common.h"
#include "value.h"
#include "chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_FILEHANDLE
} Obj_e;

struct Obj_t {
    Obj_e type;
    Obj_t *next;
};

struct ObjString_t {
    Obj_t obj;
    int length;
    bool isConst;
    uint32_t hash;
    char chars[];
};

typedef struct {
    Obj_t obj;
    int arity;
    Chunk_t chunk;
    ObjString_t *name;
} ObjFunction_t;

typedef Value_t (*NativeFn_t)(int argCount, Value_t *args);
typedef struct {
    Obj_t obj;
    int arity;
    NativeFn_t function;
} ObjNative_t;

typedef struct {
    Obj_t obj;
    const char *name;
    FILE *fh;
} ObjFileHandle_t;

#define OBJ_TYPE(value)       (AS_OBJ(value)->type)
#define IS_STRING(value)      isObjType(value, OBJ_STRING)
#define AS_STRING(value)      ((ObjString_t *)AS_OBJ(value))
#define AS_CSTRING(value)     (((ObjString_t *)AS_OBJ(value))->chars)
#define IS_FUNCTION(value)    isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value)    ((ObjFunction_t *)AS_OBJ(value))
#define IS_NATIVE(value)      isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value)      ((ObjNative_t *)AS_OBJ(value))->function
#define IS_FILEHANDLE(value)  isObjType(value, OBJ_FILEHANDLE)
#define AS_FILEHANDLE(value)  (((ObjFileHandle_t *)AS_OBJ(value)))

static inline bool isObjType(Value_t value, Obj_e type) {
    return (IS_OBJ(value) && OBJ_TYPE(value) == type);
}

ObjFunction_t *newFunction(void);
ObjNative_t *newNative(NativeFn_t function, int arity);
ObjFileHandle_t *newFileHandle(FILE *fh, const char *name);
ObjString_t *makeString(char *chars, int length);
void printObject(Value_t val);
// ObjString_t *takeString(char *chars, int length);   // create dynamic string
// ObjString_t *copyString(const char *chars, int length); // copy string from source

# endif // CLOX_OBJ_H