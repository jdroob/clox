#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_RETURN,
} OpCode_e;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *code;
    ValueArray_t constants;
} Chunk_t;

void initChunk(Chunk_t *chunk);
void writeChunk(Chunk_t *chunk, uint8_t byte);
int addConstant(Chunk_t *chunk, Value_t value);
void freeChunk(Chunk_t *chunk);

#endif // CLOX_CHUNK_H
