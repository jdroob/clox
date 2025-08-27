#ifndef CLOX_JRMALLOC_H
#define CLOX_JRMALLOC_H

#include "common.h"

// Define a platform-appropriate alignment
#ifndef JR_ALIGNMENT
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define JR_ALIGNMENT _Alignof(max_align_t)
#else
#define JR_ALIGNMENT 16  // Safe fallback for most platforms
#endif
#endif

// free chunk in a free list
typedef struct jrchunk_t jrchunk_t;
struct jrchunk_t {
    jrchunk_t *prev;
    jrchunk_t *next;
    size_t size;
} __attribute__((aligned(JR_ALIGNMENT)));

void init(void);
void *jrmalloc(size_t size);
void jrfree(void *p);
void *jrrealloc(void *ptr, size_t size);

#endif // CLOX_JRMALLOC_H