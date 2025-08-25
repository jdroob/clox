#include "common.h"
#include "jrmalloc.h"

/**
 * Roadmap for this experiment:
 *    1) Get jrmalloc working correctly
 *    2) Get jrfree working correctly
 *    3) Implement a defreagging routine
 *    4) Experiment with more optimal chunk-finding policies
 */

#ifndef DEBUG_JRMALLOC
extern uint8_t BUFFER[];
#else
uint8_t BUFFER[MAX_BUFF_LEN] = {0};
#endif
static jrchunk_t *head;
static jrchunk_t *start;

/** 
 * jrchunk format:
 *  byte0: allocated (0=yes, 1=no)
 *  byte1-8: previous free chunk (0x00000000_00000000)
 *  byte9-12: size
 *  byte 13-20: next free chunk
 *  byte 21-24: padding 
 */


/* helpers */
static void printChunk(jrchunk_t *chunk) {
    printf("chunk @ address: %p\n", chunk);
    printf("previous chunk: %p\n", chunk->prev);
    printf("chunk size: %lu\n", chunk->size);
    printf("next chunk: %p\n", chunk->next);
}

static void printFreeList(void) {
    jrchunk_t *curr = head;
    while (curr) {
        printChunk(curr);
        curr = curr->next;
    }
}

static void initChunk(jrchunk_t *chunk, jrchunk_t *prev, jrchunk_t *next) {
    chunk->prev = prev;
    // TODO: re-implement me :)
    if (!prev) chunk->size = MAX_BUFF_LEN - sizeof(jrchunk_t);
    else if (!next) chunk->size = MAX_BUFF_LEN - (chunk - start) - sizeof(jrchunk_t);
    else chunk->size = MAX_BUFF_LEN - (next - prev) - sizeof(jrchunk_t);
    chunk->next = next;
    chunk->padding = 0;
}

static jrchunk_t *findValidChunk(size_t size) {
    jrchunk_t *curr = head;
    size_t nbytes = size + sizeof(jrchunk_t);
    while (curr && (curr->size < nbytes)) curr = curr->next;
    if (!curr) {
        fprintf(stderr, "Unable to allocate new jrchunk.\n");
        exit(EXIT_FAILURE);
    }
    if (curr == head) {
        head += nbytes;
        initChunk(head, curr->prev, curr->next);
        return curr;
    } else {
        jrchunk_t *tmp = curr;
        curr += nbytes;
        initChunk(curr, curr->prev, curr->next);
        return tmp;
    }
}

/* interface functions */
void init(void) {
    start = head = (jrchunk_t *)BUFFER;
    initChunk(head, NULL, NULL);
}

void *jrmalloc(size_t size) {
    return (void *)findValidChunk(size);
}

void jrfree(void *chunk) {
    // Case 1: one chunk
    if (chunk < (void *)head) {
        jrchunk_t *tmp = head;
        head = chunk;
        initChunk(head, NULL, tmp);
        return;
    }
    // where is this chunk releative to the freeList
    jrchunk_t *first = head;
    jrchunk_t *second = head->next;
    while (first &&
           second &&
           !((void *)first <= chunk && chunk <= (void *)second)) {
            first = first->next;
            second = second->next;
    }
    initChunk(chunk, first, second);
    return;

    fprintf(stderr, "Unable to place chunk back in free list.");
    exit(EXIT_FAILURE);
}

#ifdef DEBUG_JRMALLOC
int main(void) {
    init();
    // printf("Looking for chunk of at least size 255....");
    // printChunk(findValidChunk(255));
    void *a = jrmalloc(255);
    void *b = jrmalloc(1024);
    printf("After jrmalloc'ing 2 chunks:\na: %p\nb: %p\n", a, b);
    printFreeList();
    jrfree(a);
    jrfree(b);
    printf("After jrfree'ing 2 ptrs:\na: %p\nb: %p\n", a, b);
    printFreeList();
}
#endif
