//#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "jrmalloc.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (!newSize) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (!result) {
        perror("Unable to grow array.");
        exit(EXIT_FAILURE);
    }
    return result;
}
