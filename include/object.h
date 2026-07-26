#ifndef CLOX_OBJ_H
#define CLOX_OBJ_H

#include "common.h"
#include "value.h"
#include "table.h"
#include "chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_FILEHANDLE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_LIST
} Obj_e;

struct Obj_t {
    Obj_e type;
    Obj_t *next;
    bool isMarked;
    bool isProtected;
};

struct ObjString_t {
    Obj_t obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct {
    Obj_t obj;
    int arity;
    Chunk_t chunk;
    ObjString_t *name;
    int upvalueCount;   // TODO: upvalueCount should be in ObjFunction xor ObjClosure?
    size_t upvalueCapacity;
} ObjFunction_t;

typedef struct ObjUpvalue_t ObjUpvalue_t;
typedef struct {
    Obj_t obj;
    ObjFunction_t *function;

    /**
     * Each (ObjUpvalue_t *) points to an ObjUpvalue_t object containing the
     * stack location of the upvalue.
     * 
     * Here, we declare a pointer to a dynamic array of of ObjUpvalue_t pointers. 
     */
    ObjUpvalue_t **upvalues;
    int upvalueCount;
} ObjClosure_t;

typedef Value_t (*NativeFn_t)(int argCount, Value_t *args);
typedef struct {
    Obj_t obj;
    int arity;
    NativeFn_t function;
} ObjNative_t;

typedef struct {
    Obj_t obj;
    bool isOpen;
    const char *name;
    const char *accessType;
    FILE *fh;
} ObjFileHandle_t;

typedef struct ObjUpvalue_t {
    Obj_t obj;
    Value_t *location;
    Value_t closed;
    struct ObjUpvalue_t *next;
} ObjUpvalue_t;

typedef struct {
    Obj_t obj;
    ObjString_t *name;
    Table_t methods;
} ObjClass_t;

typedef struct {
    Obj_t obj;
    ObjClass_t *klass;
    Table_t fields;
} ObjInstance_t;

typedef struct {
    Obj_t obj;
    Value_t receiver;
    ObjClosure_t *method;
} ObjBoundMethod_t;

typedef struct {
    Obj_t obj;
    ValueArray_t array;
} ObjList_t;


#define OBJ_TYPE(value)            (AS_OBJ(value)->type)
#define IS_MARKED(value)           (AS_OBJ(value)->isMarked)
#define IS_LIST(value)             isObjType(value, OBJ_LIST)
#define AS_LIST(value)             ((ObjList_t *)AS_OBJ(value))
#define IS_STRING(value)           isObjType(value, OBJ_STRING)
#define AS_STRING(value)           ((ObjString_t *)AS_OBJ(value))
#define IS_CLASS(value)            isObjType(value, OBJ_CLASS)
#define AS_CLASS(value)            ((ObjClass_t *)AS_OBJ(value))
#define IS_INSTANCE(value)         isObjType(value, OBJ_INSTANCE)
#define AS_INSTANCE(value)         ((ObjInstance_t *)AS_OBJ(value))
#define AS_CSTRING(value)          (((ObjString_t *)AS_OBJ(value))->chars)
#define IS_PROTECTED(value)        (AS_OBJ(value)->isProtected)
#define IS_CLOSURE(value)          isObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value)          ((ObjClosure_t *)AS_OBJ(value))
#define IS_FUNCTION(value)         isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value)         ((ObjFunction_t *)AS_OBJ(value))
#define IS_NATIVE(value)           isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value)           ((ObjNative_t *)AS_OBJ(value))->function
#define IS_FILEHANDLE(value)       isObjType(value, OBJ_FILEHANDLE)
#define AS_FILEHANDLE(value)       (((ObjFileHandle_t *)AS_OBJ(value)))
#define CLOSE_FILEHANDLE(value)    (AS_FILEHANDLE(value)->isOpen = false)
#define IS_FILEHANDLE_OPEN(value)  (AS_FILEHANDLE(value)->isOpen == true)
#define IS_UPVALUE(value)          isObjType(value, OBJ_UPVALUE);
#define AS_UPVALUE(value)          (((ObjUpvalue_t *)AS_OBJ(value)))
#define IS_BOUND_METHOD(value)     isObjType(value, OBJ_BOUND_METHOD)
#define AS_BOUND_METHOD(value)     (((ObjBoundMethod_t *)AS_OBJ(value)))

static inline bool isObjType(Value_t value, Obj_e type) {
    return (IS_OBJ(value) && OBJ_TYPE(value) == type);
}

ObjClosure_t *newClosure(ObjFunction_t *);
ObjFunction_t *newFunction(void);
ObjNative_t *newNative(NativeFn_t function, int arity);
ObjFileHandle_t *newFileHandle(FILE *fh, const char *name, const char *accessType);
ObjUpvalue_t *newUpvalue(Value_t *value);
ObjString_t *makeString(char *chars, int length);
ObjClass_t *newClass(ObjString_t *name);
ObjInstance_t *newInstance(ObjClass_t *klass);
ObjBoundMethod_t *newBoundMethod(Value_t receiver, ObjClosure_t *method);
ObjList_t *newList(unsigned size);
void turnOnProtectMode(Obj_t *object);
void turnOffProtectMode(Obj_t *object);
void printObject(Value_t val);
// ObjString_t *takeString(char *chars, int length);   // create dynamic string
// ObjString_t *copyString(const char *chars, int length); // copy string from source

# endif // CLOX_OBJ_H