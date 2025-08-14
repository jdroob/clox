#include "chunk.h"
#include "memory.h"

void initChunk(Chunk_t *chunk) {
    chunk->code = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
}

void writeChunk(Chunk_t *chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count++] = byte;
}

void freeChunk(Chunk_t *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    initChunk(chunk);
}
