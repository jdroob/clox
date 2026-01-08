#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"
#include "line.h"

#define OP_LONG_MAX 0x1000000
#define OP_SHORT_MAX 256
#define MASK 0x000000FF

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_ONE,
    OP_NEG_ONE,
    OP_ZERO,
    OP_NEGATE,
    OP_NOT,
    OP_GT,
    OP_LT,
    OP_EQ,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_PRINT,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_ACCESS_LOCAL,
    OP_ACCESS_GLOBAL,
    OP_ACCESS_LOCAL_LONG,
    OP_ACCESS_GLOBAL_LONG,
    OP_SET_LOCAL,
    OP_SET_GLOBAL,
    OP_SET_LOCAL_LONG,
    OP_SET_GLOBAL_LONG,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_JUMP_IF_NOT_MATCH,
    OP_JUMP,
    OP_SWITCH,
    OP_CASE,
    OP_DEFAULTCASE,
    OP_ENDSWITCH,
    OP_BREAK,
    OP_BREAKALL,
    // OP_JUMP_BACK,
    OP_LOOP,
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
unsigned writeConstant(Chunk_t *chunk, Value_t value, int line);
int addConstant(Chunk_t *chunk, Value_t value);
int getLine(Chunk_t *chunk, unsigned int offset);

#endif // CLOX_CHUNK_H
