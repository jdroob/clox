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

static bool valuesEqual(Value_t a, Value_t b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUM:  return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_NIL:  return IS_NIL(b);
        case VAL_OBJ:  {
            switch (OBJ_TYPE(a))
            {
                case OBJ_STRING:
                    return AS_STRING(a)->length == AS_STRING(b)->length &&
                            !memcmp(AS_CSTRING(a), AS_CSTRING(b), AS_STRING(a)->length);
            }
        }
        default: return false;  // unreachable
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

    ObjString_t *string = takeString(chars, length);
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

void initVM(void) {
    #ifdef JRMALLOC
    init();     // init jrmalloc
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
    resetStack();
}

void freeVM(void) {
    freeObjects();
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
    // #define READ_LONG_CONSTANT() \
    //     (vm.chunk->constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])
    #define READ_LONG_CONSTANT() \
    ({ \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        vm.chunk->constants.values[(byte2 << 16) | (byte1 << 8) | byte0]; \
    })
    #define BINARY_OP(type, op) do { \
        Value_t b = pop(); \
        Value_t a = pop(); \
        push(type##_VAL((AS_##type(a) op AS_##type(b)))); \
    } while(false)

    for (;;) {
        uint8_t instruction;
        #ifdef DEBUG
        for (Value_t *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ] ");
        }
        puts("\n");
        disassembleInstruction(vm.chunk, (unsigned)(vm.ip - vm.chunk->code));
        #endif
        switch(instruction = READ_BYTE()) {
            case OP_RETURN: {
                Value_t value = pop();
                printValue(value);
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value_t constant = READ_CONSTANT();
                push(constant);
                printValue(constant);
                printf("\n");
                break;
            }
            case OP_CONSTANT_LONG: {
                Value_t constant = READ_LONG_CONSTANT();
                push(constant);
                printValue(constant);
                printf("\n");
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
            default:
        }
    }

    #undef READ_LONG_CONSTANT
    #undef READ_CONSTANT
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
