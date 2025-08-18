#include "debug.h"
#include "value.h"

unsigned int simpleInstruction(const char *name, uint8_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static void printValue(Value_t val) {
    printf("%g", val);
}

static unsigned int constantInstruction(const char *name, Chunk_t *chunk, uint8_t offset) {
    uint8_t constantIdx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIdx);
    printValue(chunk->constants.values[constantIdx]);
    printf("'\n");
    return offset + 2;
}

unsigned int disassembleInstruction(Chunk_t *chunk, unsigned int offset) {
    printf("%04d ", offset);
    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf(" | ");
    } else {
        printf("%04d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        default:
            fprintf(stderr, "Unknown opcode %u.\n", instruction);
            return offset + 1;
    }
}

void disassembleChunk(Chunk_t *chunk, const char *name) {
    printf("==%s==\n", name);
    for (unsigned offset=0; offset<chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}
