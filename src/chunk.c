#include "chunk.h"
#include "line.h"
#include "memory.h"

#define CONSTANT_POOL_LONG_LEN_MAX 0x1000000
#define CONSTANT_POOL_SHORT_LEN_MAX 256

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

void writeConstant(Chunk_t *chunk, Value_t value, int line) {
    if (chunk->constants.capacity >= CONSTANT_POOL_LONG_LEN_MAX) {
        fprintf(stderr, "Constant pool is too large.");
        exit(EXIT_FAILURE);
    }
    int idx = addConstant(chunk, value);
    if (chunk->constants.count < CONSTANT_POOL_SHORT_LEN_MAX) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, idx, line);
        return;
    }

    //    3 least significant bytes of idx must be written to bytecode
    //    e.g. index = 257
    //         index =    0000 0000 0000 0001 0000 0001
    //         index =       0    0    0    1    0    1

    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, (unsigned)idx & 0x00FF0000, line);    // fun fact: this is big endian
    writeChunk(chunk, (unsigned)idx & 0x0000FF00, line);
    writeChunk(chunk, (unsigned)idx & 0x000000FF, line);
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
