#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

typedef double Value_t;

typedef struct {
    size_t capacity;
    size_t count;
    Value_t *values;
} ValueArray_t;

void initValueArray(ValueArray_t *array);
void writeValueArray(ValueArray_t *array, Value_t value);
void freeValueArray(ValueArray_t *array);

#endif // CLOX_VALUE_H
