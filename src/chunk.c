#include "chunk.h"
#include "line.h"
#include "memory.h"

void initChunk(Chunk_t *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initLineMap(&chunk->lines);
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk_t *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count++] = byte;
    writeLineMap(&chunk->lines, line);
}

int addConstant(Chunk_t *chunk, Value_t value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void freeChunk(Chunk_t *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeLineMap(&chunk->lines);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int getLine(Chunk_t *chunk, unsigned int offset) {
    unsigned total = 0, idx = 0;
    while (total < offset) {
        size_t freq = chunk->lines.encoding[idx].frequency;
        if (total + freq > offset) break;
        total += freq;
        idx++;
    }
    return chunk->lines.encoding[idx].line;
}
