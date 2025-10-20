#include "debug.h"
#include "value.h"

#define OPTOSTR(instruction) \
    ((instruction) == OP_RETURN ? "OP_RETURN" : \
     (instruction) == OP_NEGATE ? "OP_NEGATE" : \
     (instruction) == OP_QMARK ? "OP_QMARK" : \
     (instruction) == OP_COLON ? "OP_COLON" : \
     (instruction) == OP_ENDTERNARY ? "OP_ENDTERNARY" : \
     (instruction) == OP_ADD ? "OP_ADD" : \
     (instruction) == OP_GT ? "OP_GT" : \
     (instruction) == OP_LT ? "OP_LT" : \
     (instruction) == OP_EQ ? "OP_EQ" : \
     (instruction) == OP_NOT ? "OP_NOT" : \
     (instruction) == OP_SUBTRACT ? "OP_SUBTRACT" : \
     (instruction) == OP_MULTIPLY ? "OP_MULTIPLY" : \
     (instruction) == OP_DIVIDE ? "OP_DIVIDE" : \
     (instruction) == OP_TRUE ? "OP_TRUE" : \
     (instruction) == OP_FALSE ? "OP_FALSE" : \
     (instruction) == OP_NIL ? "OP_NIL" : \
     (instruction) == OP_CONSTANT ? "OP_CONSTANT" : \
     (instruction) == OP_CONSTANT_LONG ? "OP_CONSTANT_LONG" : \
     "UNKNOWN_INSTRUCTION")

static unsigned int simpleInstruction(const char *name, uint8_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static unsigned int constantInstruction(const char *name, Chunk_t *chunk, uint8_t offset) {
    uint8_t constantIdx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIdx);
    printValue(chunk->constants.values[constantIdx]);
    printf("'\n");
    return offset + 2;
}

static unsigned int longConstantInstruction(const char *name, Chunk_t *chunk, uint8_t offset) {
    unsigned constantIdx = 0;
    constantIdx |= chunk->code[offset + 1] << 16;
    constantIdx |= chunk->code[offset + 2] << 8;
    constantIdx |= chunk->code[offset + 3];
    printf("%-16s %4d '", name, constantIdx);
    printValue(chunk->constants.values[constantIdx]);
    printf("'\n");
    return offset + 4;
}

unsigned int disassembleInstruction(Chunk_t *chunk, unsigned int offset) {
    printf("%04d ", offset);
    if (offset > 0 &&
        getLine(chunk, offset) == getLine(chunk, offset - 1)) {
        printf(" | ");
    } else {
        printf("%04d ", getLine(chunk, offset));
    }

    uint8_t instruction = chunk->code[offset];
    char *name = OPTOSTR(instruction);
    switch (instruction) {
        case OP_RETURN:
        case OP_NEGATE:
        case OP_QMARK:
        case OP_COLON:
        case OP_ENDTERNARY:
        case OP_ADD:
        case OP_GT:
        case OP_LT:
        case OP_EQ:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_TRUE:
        case OP_FALSE:
        case OP_NIL:
        case OP_NOT:
            return simpleInstruction(name, offset);
        case OP_CONSTANT:
            return constantInstruction(name, chunk, offset);
        case OP_CONSTANT_LONG:
            return longConstantInstruction(name, chunk, offset);
        default:
            fprintf(stderr, "Unknown opcode %u.\n", instruction);
            return offset + 1;
    }
}

void printValue(Value_t val) {
    switch(val.type) {
        case VAL_BOOL:
            printf(AS_BOOL(val) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUM:
            printf("%g", AS_NUMBER(val));
            break;
    }
}

void disassembleChunk(Chunk_t *chunk, const char *name) {
    printf("==%s==\n", name);
    for (unsigned offset=0; offset<chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}
