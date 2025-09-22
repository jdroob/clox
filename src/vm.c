#include "vm.h"
#include "jrmalloc.h"
#include "debug.h"
#include "common.h"

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
    *vm.stackTop++ = value;
}

Value_t pop(void) {
    return *(--vm.stackTop);
}

void initVM(void) {
    init();     // init jrmalloc
    vm.chunk = NULL;
    vm.ip = 0;
    resetStack();
}

void freeVM(void) {

}

static InterpResult_t run(void) {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define READ_LONG_CONSTANT() \
        (vm.chunk->constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])

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
                return INTERP_OK;
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
                push(-pop());
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

    return INTERP_OK;
}

InterpResult_t interpret(Chunk_t *chunk) {
    vm.chunk = chunk;
    vm.ip = chunk->code;
    return run();
}
