#ifndef CLOX_JRMALLOC_H
#define CLOX_JRMALLOC_H

#include "common.h"

// free chunk in a free list
typedef struct jrchunk_t jrchunk_t;
struct jrchunk_t {
    jrchunk_t *prev;
    size_t size;
    jrchunk_t *next;
    int padding;
};

void init(void);
void *jrmalloc(size_t size);

#endif // CLOX_JRMALLOC_H