#include "common.h"
#include "jrmalloc.h"


/**
 * Roadmap for this experiment:
 *    1) Get jrmalloc working correctly <- DONE
 *    2) Get jrfree working correctly   <- DONE
 *    3) Implement a defragging routine    <- ON HOLD
 *    4) Experiment with more optimal chunk-finding policies    <- GOOD ENOUGH
 *    5) Implement jrrealloc        <- DONE?
 *          - if able to find big enough chunk, copy over, free orig chunk, return pointer to new chunk
 *          - if newSize is 0, free chunk and return pointer to original chunk
 *          - if origSize is 0, just treat as malloc
 *    6) In memory.c/h, implement reallocate2 that uses this allocation library    <- DONE
 */

//#ifndef DEBUG_JRMALLOC
// extern uint8_t BUFFER[];
uint8_t BUFFER[MAX_BUFF_LEN] = {0};
//#else
//uint8_t BUFFER[MAX_BUFF_LEN] = {0};
//#endif

static jrchunk_t *head, *start, *end;
#define ALLOCD_CHUNK_ARR_SIZE MAX_BUFF_LEN / sizeof(uint8_t *) 
uint8_t *allocatedChunks[ALLOCD_CHUNK_ARR_SIZE] = {0};
size_t nAllocatedChunks = 0;

/** 
 * jrchunk format:
 *  byte0-7: previous free chunk (0xXXXXXXXX_XXXXXXXX)
 *  byte8-15: next free chunk    (0xXXXXXXXX_XXXXXXXX)
 *  byte 16-19: size 
 *  byte 20-23: padding
 */


/* helpers */
#ifdef DEBUG_JRMALLOC
static void printChunk(jrchunk_t *chunk) {
    printf("chunk @ address: %p\n", chunk);
    printf("previous chunk: %p\n", chunk->prev);
    printf("chunk size: %lu\n", chunk->size);
    printf("next chunk: %p\n", chunk->next);
}

static void printFreeList(void) {
    jrchunk_t *curr = head;
    puts("===========================");
    while (curr) {
        printChunk(curr);
        curr = curr->next;
    }
    puts("===========================");
}
#endif

static size_t roundUp(size_t size) {
    size_t num = (size + JR_ALIGNMENT - 1) / JR_ALIGNMENT;
    return num * JR_ALIGNMENT;
}

static void *alignPtr(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + JR_ALIGNMENT - 1) & ~(JR_ALIGNMENT - 1);
    return (void *)aligned;
}

static void update(jrchunk_t *chunk) {
    if (chunk->prev) {
        chunk->prev->next = chunk->next;
    }
    if (chunk->next) {
        chunk->next->prev = chunk->prev;
    }
    if (chunk == head) {
        // good-bye :(
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
    size_t nbytes = size + sizeof(jrchunk_t) + JR_ALIGNMENT;
    
    while (curr && (curr->size < size)) {
        curr = curr->next;
    }
    
    if (!curr) {
        fprintf(stderr, "Unable to allocate new jrchunk.\n");
        return NULL;    // return NULL - just like realloc
    }
    
    // split the chunk if it's larger than needed
    if (curr->size - size > nbytes) {
        uint8_t *new_chunk_addr = (uint8_t *)curr + sizeof(jrchunk_t) + size;
        jrchunk_t *new_chunk = (jrchunk_t *)alignPtr(new_chunk_addr);
        insertChunk(new_chunk, curr->prev, curr->next);
        
        curr->size = size;
        curr->next = new_chunk;
        curr->prev = NULL;
        
        if (curr == head) {
            head = new_chunk;
        }
    } else {
        update(curr);
    }
    
    return curr;
}

void coalesce(void) {
    jrchunk_t *curr = head;
    while (curr && curr->next) {
        uint8_t *curr_end = (uint8_t *)curr + sizeof(jrchunk_t) + curr->size;
        if (curr_end == (uint8_t *)curr->next) {
            jrchunk_t *next_chunk = curr->next;
            curr->size += sizeof(jrchunk_t) + next_chunk->size;
            curr->next = next_chunk->next;
            if (next_chunk->next) {
                next_chunk->next->prev = curr;
            }
            // don't advance curr - check for more coalescing
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
    // printf("DEBUG: BUFFER=%p, MAX_BUFF_LEN=%d, end=%p\n", BUFFER, MAX_BUFF_LEN, end);
    // printf("DEBUG: Available buffer size: %ld bytes\n", (uint8_t*)end - (uint8_t*)start);
    insertChunk(head, NULL, NULL);
}

void *jrmalloc(size_t size) {
    uint8_t *chunkAddr = (uint8_t *)findValidChunk(roundUp(size));
    if (nAllocatedChunks > ALLOCD_CHUNK_ARR_SIZE) {
        fprintf(stderr, "Unable to jrmalloc.\n");
        exit(EXIT_FAILURE);
    }
    uint8_t *userAddr = chunkAddr + sizeof(jrchunk_t);
    userAddr = (uint8_t *)alignPtr(userAddr);
    allocatedChunks[nAllocatedChunks++] = chunkAddr;
    return (void *)userAddr;
}

void *jrrealloc(void *ptr, size_t size) {
    // case 1: NULL pointer - behave like malloc
    if (!ptr) return jrmalloc(size);

    // case 2: size = 0 - behave like free
    if (!size) {
        jrfree(ptr);
        return NULL;
    }

    jrchunk_t *chunkAddr = NULL;
    for (unsigned i=0; i<nAllocatedChunks; ++i) {
        if (allocatedChunks[i]) {
            uint8_t *chunkStart = allocatedChunks[i];
            uint8_t *userStart = chunkStart + sizeof(jrchunk_t);
            uint8_t *alignedUser = (uint8_t *)alignPtr(userStart);
            
            if (alignedUser == ptr) {
                chunkAddr = (jrchunk_t *)chunkStart;
                break;
            }
        }
    }

    // case 3: ptr is invalid (i.e. treat as malloc)
    if (!chunkAddr) return jrmalloc(size);

    // case 4: normal realloc
    // 4.1 find valid chunk
    uint8_t *newChunkAddr = (uint8_t *)findValidChunk(roundUp(size));
    if (!newChunkAddr) return NULL;    // just like real realloc
    if (nAllocatedChunks > ALLOCD_CHUNK_ARR_SIZE) {
        fprintf(stderr, "Unable to jrrealloc.\n");
        exit(EXIT_FAILURE);
    }
    uint8_t *userAddr = newChunkAddr + sizeof(jrchunk_t);
    userAddr = (uint8_t *)alignPtr(userAddr);
    allocatedChunks[nAllocatedChunks++] = newChunkAddr;

    // 4.2 copy from old ptr to new ptr
    size_t copySize = size < chunkAddr->size ? size : chunkAddr->size;
    memcpy((void *)userAddr, ptr, copySize);

    // 4.3 free old ptr
    jrfree(ptr);

    return userAddr;
}

void jrfree(void *ptr) {
    if (!ptr) return;
    jrchunk_t *chunkAddr = NULL;
    for (unsigned i=0; i<nAllocatedChunks; ++i) {
        if (allocatedChunks[i]) {
            uint8_t *chunkStart = allocatedChunks[i];
            uint8_t *userStart = chunkStart + sizeof(jrchunk_t);
            uint8_t *alignedUser = (uint8_t *)alignPtr(userStart);
            
            if (alignedUser == ptr) {
                chunkAddr = (jrchunk_t *)chunkStart;
                allocatedChunks[i] = 0;
                break;
            }
        }
    }

    if (!chunkAddr) {
        fprintf(stderr, "jrfree: invalid pointer\n");
        return;
    }
        
    // case 1: insert at beginning (chunk address < head address)
    if (chunkAddr < head) {
        insertChunk(chunkAddr, NULL, head);
        head = chunkAddr;
        coalesce();
        return;
    }
    
    // case 2: find correct position in the free list
    jrchunk_t *curr = head;
    while (curr->next && curr->next < chunkAddr) curr = curr->next;
    insertChunk(chunkAddr, curr, curr->next);
    coalesce();
}

#ifdef DEBUG_JRMALLOC
int main(void) {
    init();
    // printf("=== Initial state ===\n");
    // printf("start: %p\nend: %p\n", start, end);
    // printFreeList();

    // printf("\n=== Allocating 3 chunks ===\n");
    // printFreeList();
    // void *a = jrmalloc(10);
    // void *b = jrmalloc(20);
    // void *c = jrmalloc(30);
    // printf("a: %p, b: %p, c: %p\n", a, b, c);

    // printf("\n=== Freeing middle chunk (b) ===\n");
    // printFreeList();
    
    // printf("\n=== Freeing first chunk (a) - should coalesce ===\n");
    // jrfree(a);
    // jrfree(b);
    // printFreeList();

    // printf("\n=== Freeing last chunk (c) - should coalesce all ===\n");
    // jrfree(c);
    // printf("After freeing c:\n");
    // printFreeList();
    
    // printf("\n=== Final state after all frees ===\n");
    // printf("Expected: One large chunk of size close to %d\n", MAX_BUFF_LEN - (int)sizeof(jrchunk_t));
    // printFreeList();

    // Test jrrealloc functionality
    printf("=== Testing jrrealloc ===\n");
    
    // Test 1: Basic realloc (expand)
    printf("\n--- Test 1: Basic realloc (expand) ---\n");
    void *a = jrmalloc(8);
    printf("Allocated 8 bytes at: %p\n", a);
    
    // Write some data to verify it survives realloc
    strcpy((char*)a, "Hello");
    printf("Wrote 'Hello' to memory\n");
    puts((char *)a);
    
    a = jrrealloc(a, 16);
    printf("Reallocated to 16 bytes at: %p\n", a);
    printf("Data after realloc: '%s'\n", (char*)a);
    
    // Test 2: Realloc to smaller size
    printf("\n--- Test 2: Realloc to smaller size ---\n");
    a = jrrealloc(a, 4);
    printf("Reallocated to 4 bytes at: %p\n", a);
    printf("Data after shrink: '%s'\n", (char*)a);
    
    // Test 3: Realloc with NULL pointer (should behave like malloc)
    printf("\n--- Test 3: Realloc with NULL pointer ---\n");
    void *b = jrrealloc(NULL, 32);
    printf("jrrealloc(NULL, 32) returned: %p\n", b);
    
    // Test 4: Realloc with size 0 (should behave like free)
    printf("\n--- Test 4: Realloc with size 0 ---\n");
    void *c = jrrealloc(a, 0);
    printf("jrrealloc(a, 0) returned: %p (should be NULL)\n", c);
    
    // Test 5: Multiple reallocs
    printf("\n--- Test 5: Multiple reallocs ---\n");
    void *d = jrmalloc(16);
    strcpy((char*)d, "Test data");
    printf("Initial: %p with '%s'\n", d, (char*)d);
    
    d = jrrealloc(d, 32);
    printf("After realloc to 32: %p with '%s'\n", d, (char*)d);
    
    d = jrrealloc(d, 64);
    printf("After realloc to 64: %p with '%s'\n", d, (char*)d);
    
    d = jrrealloc(d, 8);
    printf("After realloc to 8: %p with '%s'\n", d, (char*)d);
    
    // Clean up
    jrfree(b);
    jrfree(d);
    
    // Test 6: Realloc with invalid pointer (should behave like malloc)
    printf("\n--- Test 6: Realloc with invalid pointer ---\n");
    char dummy_buffer[64];
    printf("address of dummy_buffer: %p\n", &dummy_buffer[0]);
    void *e = jrrealloc(dummy_buffer, 24);
    printf("jrrealloc(invalid_ptr, 24) returned: %p\n", e);
    if (e) {
        strcpy((char*)e, "Created new");
        printf("Successfully created new allocation: '%s'\n", (char*)e);
        jrfree(e);
    }
    
    printf("\n--- Final free list ---\n");
    printFreeList();

    return 0;
}
#endif
