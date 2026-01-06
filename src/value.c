#include <stdlib.h>
#include <string.h>
#include "value.h"
#include "vm.h"
#include "object.h"
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
        array->values = GROW_ARRAY(Value_t, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count++] = value;
}

static void boundsCheck(ValueArray_t *array, unsigned idx) {
    if (idx >= array->capacity) {
        fprintf(stderr, "value.c::writeValueArrayAt: IndexOutOfBounds!\n");
        fprintf(stderr, "Attempted to write to index %lu of array of size %lu\n", idx, array->capacity);
        exit(EXIT_FAILURE);
    }
}

void writeValueArrayAt(ValueArray_t *array, Value_t value, unsigned idx) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value_t, array->values, oldCapacity, array->capacity);
    }
    boundsCheck(array, idx);
    array->values[idx] = value;
}

Value_t getValueAt(ValueArray_t *array, unsigned idx) {
    boundsCheck(array, idx);
    return array->values[idx];
}

void freeValueArray(ValueArray_t *array) {
    FREE_ARRAY(Value_t, array->values, array->capacity);
    initValueArray(array);
}

Value_t makeValue(double val, ValueType_e type) {
    switch (type) {
        case VAL_NUM:
            return (Value_t)NUMBER_VAL(val);
        case VAL_BOOL:
            return (Value_t)BOOL_VAL(val);
        default:
            fprintf(stderr, "Error: Unknown Value type\n");
            exit(EXIT_FAILURE);
    }
}
