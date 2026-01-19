#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX  + 1))

typedef struct {
    ObjFunction_t *function;
    uint8_t *ip;    // where caller should resume
    Value_t *slots; // location in stack where this function's (callee's) locals begin
} CallFrame_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpResult_t;

typedef struct {
    size_t count;
    size_t capacity;
    bool   *isFinalFlags;
} MutableTable_t;

typedef struct {
    size_t count;
    size_t capacity;
    int    *breakJumps;
} BreakJump_t;

typedef struct {
    // Chunk_t         *chunk;
    CallFrame_t     frames[FRAMES_MAX];     // Fine w/ doing this for now - stack overflows are an expected limitation
    int             frameCount;

    // ObjFunction_t   *function;              // TODO: remove

    // uint8_t         *ip;                 // TODO: remove
    uint32_t        capacity;
    Value_t         *stack;
    Value_t         *stackTop;
    Table_t         strings;
    Table_t         globalNames;
    ValueArray_t    globalValues;
    MutableTable_t  globalIsFinals;
    Obj_t           *objects;
    int             switchCounter;  // HACK - ensure stack is in proper state after switch
} VM_t;

extern VM_t vm;

void initVM(void);
void freeVM(void);
InterpResult_t interpret(const char *source);
void push(Value_t value);
Value_t pop(void);
bool valuesEqual(Value_t a, Value_t b);
void initIsFinalsArray(MutableTable_t *array);
void freeIsFinalsArray(MutableTable_t *array);
void writeIsFinalsArray(MutableTable_t *array, bool flag);
void writeIsFinalsArrayAt(MutableTable_t *array, bool flag, unsigned idx);
void popLocalIsFinalFlag(MutableTable_t *array);
void initBreakJumpArray(BreakJump_t *array);
void writeBreakJumpArray(BreakJump_t *array, int breakJump);
void freeBreakJumpArray(BreakJump_t *array);
void resetBreakJumpArray(BreakJump_t *array);

#endif // CLOX_VM_H