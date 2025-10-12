#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "common.h"
#include "compiler.h"

#define BINARY_OP(op) do { \
    double b = pop(); \
    double a = pop(); \
    push(a op b); \
} while(false)

VM_t vm;

static void resetStack(void) {
    vm.stackTop = vm.stack;
}

void push(Value_t value) {
    if (vm.capacity < vm.stackTop - vm.stack + 1) {
        // vm.capacity *= 2;
        // void *tmp = jrrealloc(vm.stack, vm.capacity * sizeof(Value_t));
        // if (!tmp) {
        //     fprintf(stderr, "Error! VM stack overflow\n");
        //     exit(EXIT_FAILURE);
        // }
        // vm.stack = (Value_t *)tmp;
        // vm.stackTop = vm.stack + offset;
        
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
    init();     // init jrmalloc
    vm.chunk = NULL;
    vm.ip = 0;
    vm.capacity = STACK_MAX;
    vm.stack = jrmalloc(STACK_MAX * sizeof(Value_t));
    resetStack();
}

void freeVM(void) {
    FREE_ARRAY(Value_t, vm.stack, vm.capacity);
}

static InterpResult_t run(void) {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    // #define READ_LONG_CONSTANT() \
    //     (vm.chunk->constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])
    #define READ_LONG_CONSTANT() \
    ({ \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte3 = READ_BYTE(); \
        vm.chunk->constants.values[(byte1 << 16) | (byte2 << 8) | byte3]; \
    })

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
            case OP_NEGATE: {
                // TODO: move this bounds-checking logic
                if (vm.stackTop != vm.stack) *(vm.stackTop - 1) = -(*(vm.stackTop - 1));
                break;
            }
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
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
