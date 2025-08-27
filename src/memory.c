//#include <stdlib.h>
#include "common.h"
#include "memory.h"
#include "jrmalloc.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (!newSize) {
        // free(pointer);
        jrfree(pointer);
        return NULL;
    }

    // void *result = realloc(pointer, newSize);
    void *result = jrrealloc(pointer, newSize);
    if (!result) {
        perror("Unable to grow array.");
        exit(EXIT_FAILURE);
    }
    return result;
}
