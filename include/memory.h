#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCapacity, newCapacity) \
    (type *)reallocate(pointer, sizeof(type) * oldCapacity, \
                              sizeof(type) * newCapacity)

#define FREE_ARRAY(type, pointer, capacity) \
    ((type *)reallocate(pointer, sizeof(type) * capacity, 0))                              

#endif // CLOX_MEMORY_H
