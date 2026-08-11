#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX  + 1))
#define GC_HEAP_GROW_FACTOR 2
#define INIT_NEXT_GC (1024 * 1024)

typedef struct {
    ObjClosure_t *closure;
    // ObjFunction_t *function;
    uint8_t *ip;    // as the name implies - the instruction pointer
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
    bool            isInitialized;
    int             switchCounter;  // HACK - ensure stack is in proper state after switch
    int             frameCount;
    CallFrame_t     frames[FRAMES_MAX];     // Fine w/ doing this for now - stack overflows are an expected limitation
    uint32_t        capacity;
    size_t          grayCapacity;
    size_t          grayCount;
    size_t          bytesAllocated;
    size_t          nextGC;
    Value_t         *stack;
    Value_t         *stackTop;
    Table_t         strings;
    Table_t         globalNames;
    ValueArray_t    globalValues;
    MutableTable_t  globalIsFinals;
    Obj_t           *objects;
    Obj_t           **grayStack;
    ObjUpvalue_t    *openUpvalues;
    ObjString_t     *initString;
} VM_t;

extern VM_t vm;

void initVM(void);
void freeVM(void);
InterpResult_t interpret(const char *source);
void push(Value_t value);
Value_t pop(void);
bool valuesEqual(Value_t a, Value_t b);
bool isLocalFinal(MutableTable_t *array, unsigned idx);
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