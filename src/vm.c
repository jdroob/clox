#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "common.h"
#include "compiler.h"
#include "value.h"
#include "object.h"


VM_t vm;

static void resetStack(void) {
    vm.stackTop = vm.stack;
}

void freeVM(void);
static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instructionOffset = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, instructionOffset);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
    //freeVM(); // freeing here will result in double free in main
}

void initIsFinalsArray(MutableTable_t *array) {
    array->capacity = 0;
    array->count = 0;
    array->isFinalFlags = NULL;
}

void freeIsFinalsArray(MutableTable_t *array) {
    array->capacity = 0;
    array->count = 0;
    #ifdef JRMALLOC
    jrfree(array->isFinalFlags);
    #else
    free(array->isFinalFlags);
    #endif
    array->isFinalFlags = NULL;
}

void writeIsFinalsArray(MutableTable_t *array, bool flag) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->isFinalFlags = GROW_ARRAY(bool, array->isFinalFlags, oldCapacity, array->capacity);
    }
    array->isFinalFlags[array->count++] = flag;
}

void writeIsFinalsArrayAt(MutableTable_t *array, bool flag, unsigned idx) {
    // PRECONDITION: idx < array->count
    //   This function should only ever be called to update an existing isFinals flag
    array->isFinalFlags[idx] = flag;
}

bool isLocalFinal(MutableTable_t *array, unsigned idx) {
    return array->isFinalFlags[idx];
}

void popLocalIsFinalFlag(MutableTable_t *array) {
    if (array->count) array->count--;
}

void initBreakJumpArray(BreakJump_t *array) {
    array->count = 0;
    array->capacity = 0;
    array->breakJumps = NULL;
}

void writeBreakJumpArray(BreakJump_t *array, int breakJump) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->breakJumps = GROW_ARRAY(int, array->breakJumps, oldCapacity, array->capacity);
    }
    array->breakJumps[array->count++] = breakJump;
}

void freeBreakJumpArray(BreakJump_t *array) {
    array->capacity = 0;
    array->count = 0;
    #ifdef JRMALLOC
    jrfree(array->breakJumps);
    #else
    free(array->breakJumps);
    #endif
    array->breakJumps = NULL;
}

void resetBreakJumpArray(BreakJump_t *array) {
    array->count = 0;
}

static Value_t peek(int distance) {
    return vm.stackTop[-1 - distance];
}

// static bool isFalsey(Value_t value) {
//     return (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
// }

static bool isTruthy(Value_t value) {
    return (IS_NUMBER(value) ? value.as.num != 0 : IS_BOOL(value) ? AS_BOOL(value) : false);
}

static bool isFalsey(Value_t value) {
    switch (value.type) {
        case VAL_BOOL: return !AS_BOOL(value);
        case VAL_NUM: return AS_NUMBER(value) == 0;
        case VAL_NIL: return true;
        case VAL_OBJ: {
            switch(OBJ_TYPE(value)) {
                case OBJ_STRING:
                    return AS_STRING(value)->length == 0;
            }
        }
        default: return false;
    }
}

bool valuesEqual(Value_t a, Value_t b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUM:  return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_NIL:  return IS_NIL(b);
        case VAL_EMPTY: return true;
        case VAL_OBJ:  {
            switch (OBJ_TYPE(a))
            {
                case OBJ_STRING:
                    return AS_STRING(a)->length == AS_STRING(b)->length &&
                           AS_STRING(a)->hash == AS_STRING(b)->hash &&
                            !memcmp(AS_CSTRING(a), AS_CSTRING(b), AS_STRING(a)->length);
            }
        }
        default: return false;
    }
}

static void concatenate(void) {
    ObjString_t *b = AS_STRING(pop());
    ObjString_t *a = AS_STRING(pop());

    size_t length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString_t *string = makeString(chars, length);
    FREE(char, chars);
    push(OBJ_VAL(string));
}

static void concatenateNum(void) {
    Value_t b = pop();
    Value_t a = pop();

    ObjString_t *str;
    double num;
    bool bIsString;
    str = IS_STRING(b) ? (bIsString = true, num = AS_NUMBER(a), AS_STRING(b)) : \ 
        (bIsString = false, num = AS_NUMBER(b), AS_STRING(a));

    char *result = NULL;
    size_t len = str->length;
    #include <math.h>
    bool hasDecimalPart = fmod(num, 1.0) != 0.0;
    int truncated = (int)num;

    // Calculate total length
    if (hasDecimalPart) {
        len += snprintf(NULL, 0, "%g", num) + 1;
    } else {
        len += snprintf(NULL, 0, "%d", truncated) + 1;
    }

    result = (char *)malloc(len);
    if (!result) {
        runtimeError("Memory allocation failed for concatenation.\n");
        return;
    }

    if (hasDecimalPart) {
        // No decimal part
        if (bIsString) {
            snprintf(result, len, "%g%s", num, str->chars);
        } else {
            snprintf(result, len, "%s%g", str->chars, num);
        }
    } else {
        if (bIsString) {
            snprintf(result, len, "%d%s", truncated, str->chars);
        } else {
            snprintf(result, len, "%s%d", str->chars, truncated);
        }
    }

    // TODO: Confirm no memory leak
    push(OBJ_VAL(makeString(result, len - 1)));
    free(result);
}

void push(Value_t value) {
    if (vm.capacity < vm.stackTop - vm.stack + 1) {
        uint32_t oldCapacity = vm.capacity;
        off_t offset = vm.stackTop - vm.stack;
        vm.capacity = GROW_CAPACITY(vm.capacity);
        vm.stack = GROW_ARRAY(Value_t, vm.stack, oldCapacity, vm.capacity);
        vm.stackTop = vm.stack + offset;
    }
    *vm.stackTop++ = value;
}

static void writeStackAt(unsigned idx, Value_t value) {
    *(vm.stackTop - 1 - idx) = value;
}

static Value_t getStackAt(unsigned idx) {
    return *(vm.stackTop - 1 - idx);
}

Value_t pop(void) {
    if (vm.stackTop == vm.stack) return;
    return *(--vm.stackTop);
}

void initVM(void) {
    #ifdef JRMALLOC
    init(); // init jrmalloc
    #endif
    vm.chunk = NULL;
    vm.ip = 0;
    vm.capacity = STACK_MAX;
    vm.switchCounter = 0;
    #ifdef JRMALLOC
    vm.stack = jrmalloc(STACK_MAX * sizeof(Value_t));
    #else
    vm.stack = ALLOCATE(Value_t, STACK_MAX);
    #endif
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globalNames);
    initValueArray(&vm.globalValues);
    initIsFinalsArray(&vm.globalIsFinals);
    resetStack();
}

void freeVM(void) {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globalNames);
    freeValueArray(&vm.globalValues);
    freeIsFinalsArray(&vm.globalIsFinals);
    FREE_ARRAY(Value_t, vm.stack, vm.capacity);
}

void updateObjList(Obj_t *obj) {
    if (!vm.objects) {  // empty object list
        vm.objects = obj;
        obj->next = NULL;
        return;
    }
    Obj_t *curr = vm.objects;
    while (curr->next) curr = curr->next;
    curr->next = obj;
    obj->next = NULL;
}

static InterpResult_t run(void) {
    #define READ_BYTE() (*vm.ip++)
    #define READ_BYTES() \
    ({ \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        unsigned bytes = (byte2 << 16) | (byte1 << 8) | byte0;  \
        bytes;  \
    })
    #define READ_SHORT() \
    ({ \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        uint16_t bytes = (byte1 << 8) | byte0; \
        bytes; \
    })
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    /**
     * NOTE: below is a "statement expression"
     *  syntax:
     *      { stmt0; stmt1; ...; stmtN; expession; }
     *  The net effect is an "expression" that returns a value and has 0 or more side-effects.
     * 
     * NOTE: statement expressions are supported in GCC and Clang but NOT all C compilers support them.
    */
    #define READ_CONSTANT_LONG() \
    ({ \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        vm.chunk->constants.values[(byte2 << 16) | (byte1 << 8) | byte0]; \
    })
    #define READ_STRING() (AS_STRING(READ_CONSTANT()))
    #define READ_STRING_LONG() (AS_STRING(READ_CONSTANT_LONG()))
    #define BINARY_OP(resType, operandType, op) do { \
        Value_t b = pop(); \
        Value_t a = pop(); \
        push(resType##_VAL((AS_##operandType(a) op AS_##operandType(b)))); \
    } while(false)
    #define IS_FINAL(idx) (vm.globalIsFinals.isFinalFlags[idx])

    for (;;) {
        uint8_t instruction;
        #ifdef DEBUG
        appendNewline = false;
        for (Value_t *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ] ");
        }
        puts("\n");
        disassembleInstruction(vm.chunk, (unsigned)(vm.ip - vm.chunk->code));
        appendNewline = true;
        #endif
        switch(instruction = READ_BYTE()) {
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value_t constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value_t constant = READ_CONSTANT_LONG();
                push(constant);
                break;
            }
            case OP_TRUE: {
                push(BOOL_VAL(true));
                break;
            }
            case OP_FALSE: {
                push(BOOL_VAL(false));
                break;
            }
            case OP_ZERO: {
                push(NUMBER_VAL(0));
                break;
            }
            case OP_ONE: {
                push(NUMBER_VAL(1));
                break;
            }
            case OP_NEG_ONE: {
                push(NUMBER_VAL(-1));
                break;
            }
            case OP_NIL: {
                push(NIL_VAL);
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (vm.stackTop != vm.stack) 
                    (vm.stackTop - 1)->as.num = -AS_NUMBER(*(vm.stackTop - 1));
                break;
            }
            case OP_NOT: {
                push(BOOL_VAL(isFalsey(pop())));
                break;
            }
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    BINARY_OP(NUMBER, NUMBER, +); 
                } else if (IS_NUMSTR(peek(0), peek(1))) {
                    concatenateNum();
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER, NUMBER, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER, NUMBER, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER, NUMBER, /); break;
            case OP_GT:         BINARY_OP(BOOL, NUMBER, >); break;
            case OP_LT:         BINARY_OP(BOOL, NUMBER, <); break;
            case OP_MODULO: {
                Value_t b = pop();
                Value_t a = pop();
                double res = (double)((int)AS_NUMBER(a) % (int)AS_NUMBER(b));
                push(NUMBER_VAL(res));
                break;
            }
            case OP_EQ: {
                Value_t b = pop();
                Value_t a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_POP: {
                pop();
                break;
            }
            case OP_PRINT: {
                printValue(pop());
                break;
            }
            case OP_DEFINE_GLOBAL: 
            case OP_DEFINE_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_DEFINE_GLOBAL) {
                    idx = (unsigned)READ_BYTE();  // read 1-byte index into vm.globalValues
                } else {
                    idx = READ_BYTES(); // read 3-byte index into vm.globalValues
                }
                Value_t value = pop();
                writeValueArrayAt(&vm.globalValues, value, idx);
                break;
            }
            case OP_ACCESS_GLOBAL:
            case OP_ACCESS_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_ACCESS_GLOBAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }
                Value_t value = getValueAt(&vm.globalValues, idx);
                if (IS_UNDEFINED(value)) {
                    runtimeError("Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL:
            case OP_SET_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_SET_GLOBAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }

                /**
                 * Like C, the expression <identifier> = <value>
                 *  produces the value <value>. Thus, <value> must be
                 *  at the top of the stack after the assignment is complete.
                 *  We *could* do something like: pop, add to table, push... but why?
                 *  Instead, just peek at entry in globals valueArray and be leave the
                 *  stack alone (since that's the net effect anyway)
                 */
                
                // Should have been set to NIL or defined value by this point
                if (IS_UNDEFINED(getValueAt(&vm.globalValues, idx))) {
                    runtimeError("Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_FINAL(idx)) {
                    runtimeError("Cannot assign to 'final' variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                writeValueArrayAt(&vm.globalValues, peek(0), idx);
                break;
            }
            case OP_ACCESS_LOCAL:
            case OP_ACCESS_LOCAL_LONG: {
                unsigned idx;
                if (instruction == OP_ACCESS_LOCAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }
                push(vm.stack[idx]);
                break;
            }
            case OP_SET_LOCAL:
            case OP_SET_LOCAL_LONG: {
                unsigned idx;
                if (instruction == OP_SET_LOCAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }

                // writeStackAt(idx, peek(0));
                vm.stack[idx] = peek(0);
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_SHORT();
                if (!isFalsey(peek(0))) {
                    vm.ip += offset;
                    break;
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) {
                    vm.ip += offset;
                    break;
                }

                break;

                // Value_t value = peek(0);
                // if (falsey(value)) {
                //     uint16_t offset = 0;
                //     offset = (offset | *vm.ip) << 8;
                //     offset = offset | *(vm.ip + 1);
                //     vm.ip += offset;
                //     break;
                // }
                // vm.ip += 2; // Skip offset
                // break;
            }
            // There need not be 2 ops for this
            case OP_BREAK:
            case OP_BREAKALL: {
                unsigned popCount = READ_BYTES();
                while (popCount--) pop();
                break;
            }
            case OP_SWITCH: {
                vm.switchCounter++;   // account for switch expression
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            case OP_CASE: {
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                vm.switchCounter++; // to account for case expression
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            // case OP_DEFAULTCASE: {
            //     printf("vm.switchCounter: %d\n", vm.switchCounter);
            //     break;
            // }
            case OP_ENDSWITCH: {
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                if (vm.switchCounter < 0) runtimeError("Stack in invalid state post-switch.");
                
                /**
                 * precondition: vm.switchCounter >= prevSwitchDepth
                 * 
                 * For each 'switch', vm.switchCounter was incremented
                 * Thus, when exiting a 'switch', we only want to decrement
                 * to the previous switch depth (not necessarily 0).
                 */
                uint8_t prevSwitchDepth = READ_BYTE();
                while (vm.switchCounter > prevSwitchDepth) {
                    pop();
                    vm.switchCounter--;
                }
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            case OP_JUMP_IF_NOT_MATCH: {
                uint16_t offset = READ_SHORT();
                if (valuesEqual(peek(0), peek(1))) {
                    pop();  // pop case expression
                    pop();  // pop switch expression
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    vm.switchCounter -= 2;
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                } else {
                    pop();  // pop case expression
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    vm.switchCounter--;
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    vm.ip += offset;  // jump to next case or default
                }
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm.ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm.ip -= offset;
                break;
            }
            default:
        }
    }

    #undef READ_CONSTANT_LONG
    #undef READ_CONSTANT
    #undef READ_STRING_LONG
    #undef READ_STRING
    #undef READ_BYTE
    #undef READ_SHORT
    #undef IS_FINAL

    return INTERPRET_OK;
}

InterpResult_t interpret(const char *source) {
    Chunk_t chunk;
    initChunk(&chunk);
    
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }
    
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpResult_t result = run();

    freeChunk(&chunk);
    return result;
}
