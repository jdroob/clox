#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"

typedef enum {
    OP_RETURN,
} OpCode_e;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *code;
} Chunk_t;

void initChunk(Chunk_t *chunk);
void freeChunk(Chunk_t *chunk);
void writeChunk(Chunk_t *chunk, uint8_t byte);

#endif // CLOX_CHUNK_H
