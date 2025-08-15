#include <stdlib.h>
#include "value.h"
#include "memory.h"

void initValueArray(ValueArray_t *array) {
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}

void writeValueArray(ValueArray_t *array, Value_t value) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value_t, array, oldCapacity, array->capacity);
    }
    array->values[array->count++] = value;
}

void freeValueArray(ValueArray_t *array) {
    FREE_ARRAY(Value_t, array->values, array->capacity);
    initValueArray(array);
}
