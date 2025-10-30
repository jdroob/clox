#include "common.h"
#include "memory.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (!newSize) {
        #ifdef JRMALLOC
        jrfree(pointer);
        #else
        free(pointer);
        #endif
        return NULL;
    }

    #ifdef JRMALLOC
    void *result = jrrealloc(pointer, newSize);
    #else
    void *result = realloc(pointer, newSize);
    #endif
    if (!result) {
        perror("Unable to grow array.");
        exit(EXIT_FAILURE);
    }
    return result;
}
