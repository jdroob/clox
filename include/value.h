#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

// Experiment: Try to avoid re-adding same values
#define MRU_SW 8    // Most Recently Used Sliding Window

typedef struct Obj_t Obj_t;
typedef struct ObjString_t ObjString_t;
typedef enum {
    VAL_NUM,
    VAL_BOOL,
    VAL_OBJ,
    VAL_NIL,
    VAL_EMPTY, // Internal empty type
} ValueType_e;

typedef struct {
    ValueType_e type;
    union {
        bool boolean;
        double num;
        Obj_t *obj;
    } as;
} Value_t;

#define BOOL_VAL(value)   ((Value_t){.type = VAL_BOOL, .as.boolean = value})
#define NUMBER_VAL(value) ((Value_t){.type = VAL_NUM, .as.num = value})
#define OBJ_VAL(object)   ((Value_t){.type = VAL_OBJ, .as.obj = object})
#define NIL_VAL           ((Value_t){.type = VAL_NIL, .as.num = 0})
#define EMPTY_VAL         ((Value_t){.type = VAL_EMPTY, .as.num = 0})

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.num)
#define AS_OBJ(value)    ((value).as.obj)

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_EMPTY(value)  ((value).type == VAL_EMPTY)

typedef struct {
    size_t capacity;
    size_t count;
    Value_t *values;
} ValueArray_t;

void initValueArray(ValueArray_t *array);
unsigned writeValueArray(ValueArray_t *array, Value_t value);
void freeValueArray(ValueArray_t *array);
Value_t makeValue(double val, ValueType_e type);

#endif // CLOX_VALUE_H
