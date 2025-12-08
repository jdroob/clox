#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define STACK_MAX 256
// Can tune this based on how much linear probing you think will take place on each tableGet() call
// Goal of cache is to (usually) return value with fewer misses than would take place in linear probing
#define CACHE_SIZE 3

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpResult_t;

typedef struct {
    Chunk_t *chunk;
    uint8_t *ip;
    uint32_t capacity;
    Value_t *stack;
    Value_t *stackTop;
    Table_t strings;
    Table_t globalNames;
    ValueArray_t globalValues;
    Obj_t *objects;
    Entry_t cache[CACHE_SIZE];
} VM_t;

extern VM_t vm;

void initVM(void);
void freeVM(void);
InterpResult_t interpret(const char *source);
void push(Value_t value);
Value_t pop(void);
bool valuesEqual(Value_t a, Value_t b);

#endif // CLOX_VM_H