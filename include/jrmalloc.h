#ifndef CLOX_JRMALLOC_H
#define CLOX_JRMALLOC_H

#include "common.h"

// free chunk in a free list
typedef struct jrchunk_t jrchunk_t;
struct jrchunk_t {
    jrchunk_t *prev;
    jrchunk_t *next;
    size_t size;
};

void init(void);
void *jrmalloc(size_t size);
void jrfree(void *p);

#endif // CLOX_JRMALLOC_H