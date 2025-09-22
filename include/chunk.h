#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"
#include "line.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
} OpCode_e;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *code;
    LineMap_t lines;
    ValueArray_t constants;
} Chunk_t;

void initChunk(Chunk_t *chunk);
void writeChunk(Chunk_t *chunk, uint8_t byte, int line);
void freeChunk(Chunk_t *chunk);
void writeConstant(Chunk_t *chunk, Value_t value, int line);
int addConstant(Chunk_t *chunk, Value_t value);
int getLine(Chunk_t *chunk, unsigned int offset);

#endif // CLOX_CHUNK_H
