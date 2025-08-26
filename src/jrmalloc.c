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
static jrchunk_t *end;

#define ALLOCD_CHUNK_ARR_SIZE MAX_BUFF_LEN / sizeof(uint8_t *) 
uint8_t *allocatedChunks[ALLOCD_CHUNK_ARR_SIZE] = {0};
size_t nAllocatedChunks = 0;

/** 
 * jrchunk format:
 *  byte0: allocated (0=yes, 1=no)
 *  byte1-8: previous free chunk (0x00000000_00000000)
 *  byte9-12: size
 *  byte 13-20: next free chunk
 *  byte 21-32: padding
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

static void update(jrchunk_t *chunk) {
    // Remove chunk from free list by updating pointers
    if (chunk->prev) {
        chunk->prev->next = chunk->next;
    }
    if (chunk->next) {
        chunk->next->prev = chunk->prev;
    }
    // If this was the head, update head pointer
    if (chunk == head) {
        head = chunk->next;
    }
}

static void insertChunk(jrchunk_t *chunk, jrchunk_t *prev, jrchunk_t *next) {
    chunk->prev = prev;
    chunk->next = next;
    chunk->size = 0;
    
    uint8_t *chunk_start = (uint8_t *)chunk + sizeof(jrchunk_t);
    uint8_t *candidate = next ? (uint8_t *)next : (uint8_t *)end;
    for (unsigned i=0; i<nAllocatedChunks; ++i) {
        if (candidate > allocatedChunks[i] && chunk_start < allocatedChunks[i]) {
            candidate = allocatedChunks[i];
        }
    }
    uint8_t *chunk_end = candidate;
    if (chunk_start > (uint8_t *)end || chunk_end > (uint8_t *)end) {
        fprintf(stderr, "Unable to grow buffer.\n");
        exit(EXIT_FAILURE);
    }
    chunk->size = chunk_end - chunk_start;
    
    if (prev) prev->next = chunk;
    if (next) next->prev = chunk;
}

static jrchunk_t *findValidChunk(size_t size) {
    jrchunk_t *curr = head;
    size_t nbytes = size + sizeof(jrchunk_t);
    
    // Find a chunk with enough space
    while (curr && (curr->size < size)) {
        curr = curr->next;
    }
    
    if (!curr) {
        fprintf(stderr, "Unable to allocate new jrchunk.\n");
        exit(EXIT_FAILURE);
    }
    
    // Split the chunk if it's larger than needed
    if (curr->size - size > nbytes) {
        // Create a new chunk for the remaining space
        jrchunk_t *new_chunk = (jrchunk_t *)((uint8_t *)curr + nbytes);
        insertChunk(new_chunk, curr->prev, curr->next);
        
        // Update the current chunk
        curr->size = size;
        curr->next = new_chunk;
        curr->prev = NULL; // This chunk is now allocated
        
        // Update head if necessary
        if (curr == head) {
            head = new_chunk;
        }
    } else {
        // Use the entire chunk
        update(curr);
    }
    
    return curr;
}

void coalesce(void) {
    jrchunk_t *curr = head;
    while (curr && curr->next) {
        // Check if current chunk is adjacent to next chunk
        uint8_t *curr_end = (uint8_t *)curr + sizeof(jrchunk_t) + curr->size;
        if (curr_end == (uint8_t *)curr->next) {
            // Merge curr with curr->next
            jrchunk_t *next_chunk = curr->next;
            curr->size += sizeof(jrchunk_t) + next_chunk->size;
            curr->next = next_chunk->next;
            if (next_chunk->next) {
                next_chunk->next->prev = curr;
            }
            // Don't advance curr, check for more coalescing
        } else {
            curr = curr->next;
        }
    }
}

void updateAllocatedChunks(uint8_t *addr) {
    for (unsigned i=0; i<nAllocatedChunks; ++i) {
        if (addr == allocatedChunks[i]) {
            allocatedChunks[i] = 0;
            return;
        }
    }
}

/* interface functions */
void init(void) {
    start = head = (jrchunk_t *)BUFFER;
    end = (jrchunk_t *)(BUFFER + MAX_BUFF_LEN);
    printf("DEBUG: BUFFER=%p, MAX_BUFF_LEN=%d, end=%p\n", BUFFER, MAX_BUFF_LEN, end);
    printf("DEBUG: Available buffer size: %ld bytes\n", (uint8_t*)end - (uint8_t*)start);
    insertChunk(head, NULL, NULL);
}

void *jrmalloc(size_t size) {
    uint8_t *addr = (uint8_t *)findValidChunk(size);
    if (nAllocatedChunks > ALLOCD_CHUNK_ARR_SIZE) {
        fprintf(stderr, "Unable to jrmalloc.\n");
        exit(EXIT_FAILURE);
    }
    allocatedChunks[nAllocatedChunks++] = addr;
    return (void *)addr;
}

void jrfree(void *chunk) {
    updateAllocatedChunks((uint8_t *)chunk);
    
    jrchunk_t *free_chunk = (jrchunk_t *)chunk;
    
    // Case 1: Insert at beginning (chunk address < head address)
    if (chunk < (void *)head) {
        insertChunk(free_chunk, NULL, head);
        head = free_chunk;
        coalesce();
        return;
    }
    
    // Case 2: Find correct position in the free list
    jrchunk_t *curr = head;
    while (curr->next && curr->next < free_chunk) {
        curr = curr->next;
    }
    
    // Insert after curr
    insertChunk(free_chunk, curr, curr->next);
    coalesce();
}

#ifdef DEBUG_JRMALLOC
int main(void) {
    init();
    printf("=== Initial state ===\n");
    printf("start: %p\nend: %p\n", start, end);
    printFreeList();

    printf("\n=== Allocating 3 chunks ===\n");
    printFreeList();
    void *a = jrmalloc(10);
    void *b = jrmalloc(20);
    void *c = jrmalloc(30);
    printf("a: %p, b: %p, c: %p\n", a, b, c);

    printf("\n=== Freeing middle chunk (b) ===\n");
    printFreeList();
    
    printf("\n=== Freeing first chunk (a) - should coalesce ===\n");
    jrfree(a);
    jrfree(b);
    printFreeList();

    printf("\n=== Freeing last chunk (c) - should coalesce all ===\n");
    jrfree(c);
    printf("After freeing c:\n");
    printFreeList();
    
    printf("\n=== Final state after all frees ===\n");
    printf("Expected: One large chunk of size close to %d\n", MAX_BUFF_LEN - (int)sizeof(jrchunk_t));
    printFreeList();
}
#endif
