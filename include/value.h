#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

typedef enum {
    VAL_NUM,
    VAL_BOOL,
    VAL_NIL,
    VAL_STR
} ValueType_e;

typedef struct {
    ValueType_e type;
    union {
        bool boolean;
        double num;
    } as;
} Value_t;

#define BOOL_VAL(value)   ((Value_t){.type = VAL_BOOL, .as.boolean = value})
#define NIL_VAL           ((Value_t){.type = VAL_NIL, .as.num = 0})
#define NUMBER_VAL(value) ((Value_t){.type = VAL_NUM, .as.num = value})

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.num)

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_NIL(value)    ((value).type == VAL_NIL)

// typedef double Value_t;

typedef struct {
    size_t capacity;
    size_t count;
    Value_t *values;
} ValueArray_t;

void initValueArray(ValueArray_t *array);
void writeValueArray(ValueArray_t *array, Value_t value);
void freeValueArray(ValueArray_t *array);
Value_t makeValue(double val, ValueType_e type);

#endif // CLOX_VALUE_H
