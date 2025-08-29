#include "vm.h"
#include "jrmalloc.h"
#include "debug.h"
#include "common.h"

VM_t vm;

void initVM(void) {
    init();
    vm.chunk = NULL;
    vm.ip = 0;
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
        disassembleInstruction(vm.chunk, (unsigned)(vm.ip - vm.chunk->code));
        #endif
        switch(instruction = READ_BYTE()) {
            case OP_RETURN: {
                return INTERP_OK;
            }
            case OP_CONSTANT: {
                Value_t value = READ_CONSTANT();
                printValue(value);
                printf("\n");
                break;
            }
            case OP_CONSTANT_LONG: {
                Value_t value = READ_LONG_CONSTANT();
                printValue(value);
                printf("\n");
                break;
            }
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
