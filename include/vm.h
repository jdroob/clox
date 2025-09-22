#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef enum {
    INTERP_OK,
    INTERP_COMP_ERR,
    INTERP_RUNTIME_ERR
} InterpResult_t;

typedef struct {
    Chunk_t *chunk;
    uint8_t *ip;
    Value_t stack[STACK_MAX];
    Value_t *stackTop;
} VM_t;

void initVM(void);
void freeVM(void);
InterpResult_t interpret(Chunk_t *chunk);
void push(Value_t value);
Value_t pop(void);

#endif // CLOX_VM_H