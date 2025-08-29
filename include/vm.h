#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"

typedef enum {
    INTERP_OK,
    INTERP_COMP_ERR,
    INTERP_RUNTIME_ERR
} InterpResult_t;

typedef struct {
    Chunk_t *chunk;
    uint8_t *ip;
} VM_t;

void initVM(void);
void freeVM(void);
InterpResult_t interpret(Chunk_t *chunk);

#endif // CLOX_VM_H