#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "common.h"
#include "compiler.h"
#include "value.h"
#include "object.h"

VM_t vm;
uint8_t nextWrite = 0;

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
    freeVM();
}

static Value_t peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value_t value) {
    return (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
}

static bool isTruthy(Value_t value) {
    return (IS_NUMBER(value) ? value.as.num != 0 : IS_BOOL(value) ? AS_BOOL(value) : false);
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

Value_t pop(void) {
    return *(--vm.stackTop);
}

static void initCache(Entry_t *cache) {
    for (uint8_t i=0; i<CACHE_SIZE; ++i) {
        cache[i].key = EMPTY_VAL;
        cache[i].value = NIL_VAL;
    }
}

static int readCache(Value_t key) {
    for (uint8_t i=0; i<CACHE_SIZE; ++i) {
        if (valuesEqual(vm.cache[i].key, key)) return i;
    }
    return -1;
}

static void writeCache(Value_t key, Value_t value) {
    for (uint8_t i=0; i<CACHE_SIZE; ++i) {
        if (valuesEqual(vm.cache[i].key, key)) {
            vm.cache[i].value = value;
            return;
        }
    }
    vm.cache[nextWrite].key = key;
    vm.cache[nextWrite].value = value;
    nextWrite = (nextWrite + 1) % CACHE_SIZE;
}

void initVM(void) {
    #ifdef JRMALLOC
    init(); // init jrmalloc
    #endif
    vm.chunk = NULL;
    vm.ip = 0;
    vm.capacity = STACK_MAX;
    #ifdef JRMALLOC
    vm.stack = jrmalloc(STACK_MAX * sizeof(Value_t));
    #else
    vm.stack = ALLOCATE(Value_t, STACK_MAX);
    #endif
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);
    initCache(&vm.cache);
    resetStack();
}

void freeVM(void) {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
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
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    /**
     * NOTE: below is a "statement expressions"
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
    #define BINARY_OP(type, op) do { \
        Value_t b = pop(); \
        Value_t a = pop(); \
        push(type##_VAL((AS_##type(a) op AS_##type(b)))); \
    } while(false)

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
                // Value_t value = pop();
                // printValue(value);
                // printf("\n");
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value_t constant = READ_CONSTANT();
                push(constant);
                // printValue(constant);
                // printf("\n");
                break;
            }
            case OP_CONSTANT_LONG: {
                Value_t constant = READ_CONSTANT_LONG();
                push(constant);
                // printValue(constant);
                // printf("\n");
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
                    BINARY_OP(NUMBER, +); 
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER, /); break;
            case OP_GT:         BINARY_OP(BOOL, >); break;
            case OP_LT:         BINARY_OP(BOOL, <); break;
            case OP_EQ: {
                Value_t b = pop();
                Value_t a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_QMARK: {
                Value_t condition = pop();
                if (!isTruthy(condition)) {
                    unsigned char depth = 1;
                    while (depth) {
                        uint8_t op = READ_BYTE();
                        if (op == OP_CONSTANT) {
                            READ_BYTE();    // discard const pool idx
                        }
                        if (op == OP_CONSTANT_LONG) {
                            READ_BYTE(); // discard byte2
                            READ_BYTE(); // discard byte1
                            READ_BYTE(); // discard byte0
                        }
                        if (op == OP_QMARK) depth++;
                        if (op == OP_COLON) depth--;
                    }
                }
                break;
            }
            case OP_COLON: {  // whenever we're here, we've already executed the true branch... so skip false branch
                while ((instruction = READ_BYTE()) != OP_ENDTERNARY) {
                    if (instruction == OP_CONSTANT) {
                        READ_BYTE();    // discard const pool idx
                    }
                    if (instruction == OP_CONSTANT_LONG) {
                        READ_BYTE(); // discard byte2
                        READ_BYTE(); // discard byte1
                        READ_BYTE(); // discard byte0
                    }
                }
            }
            case OP_ENDTERNARY: break;
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
                ObjString_t *name;
                if (instruction == OP_DEFINE_GLOBAL) {
                    name = READ_STRING();
                } else {
                    name = READ_STRING_LONG();
                }
                Value_t value = pop();
                tableSet(&vm.globals, OBJ_VAL(name), value);
                writeCache(OBJ_VAL(name), value);
                break;
            }
            case OP_ACCESS_GLOBAL:
            case OP_ACCESS_GLOBAL_LONG: {
                ObjString_t *name;
                if (instruction == OP_ACCESS_GLOBAL) {
                    name = READ_STRING();
                } else {
                    name = READ_STRING_LONG();
                }
                int idx;
                Value_t value;
                if ((idx = readCache(OBJ_VAL(name))) != -1) {
                    value = vm.cache[idx].value;
                } else {
                    if (!tableGet(&vm.globals, OBJ_VAL(name), &value)) {
                        runtimeError("Variable: %s not found.\n", name->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL:
            case OP_SET_GLOBAL_LONG: {
                ObjString_t *name;
                if (instruction == OP_SET_GLOBAL) {
                    name = READ_STRING();
                } else {
                    name = READ_STRING_LONG();
                }

                /**
                 * Like C, the expression <identifier> = <value>
                 *  produces the value <value>. Thus, <value> must be
                 *  at the top of the stack after the assignment is complete.
                 *  We *could* do the below (pop, add to table, push)... but why?
                 *  Instead, just peek when you add entry to globals table and leave
                 *  stack alone (since that's the net effect anyway)
                 */
                
                // Value_t value = pop();
                // if (tableSet(&vm.globals, OBJ_VAL(name), value)) {
                //     tableDelete(&vm.globals, OBJ_VAL(name));
                //     runtimeError("Variable: %s not declared.\n", name->chars);
                //     return INTERPRET_RUNTIME_ERROR;
                // }
                // push(value);
                writeCache(OBJ_VAL(name), peek(0));
                if (tableSet(&vm.globals, OBJ_VAL(name), peek(0))) {
                    tableDelete(&vm.globals, OBJ_VAL(name));
                    runtimeError("Variable: %s not declared.\n", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
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
