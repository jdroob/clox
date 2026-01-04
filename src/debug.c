#include "debug.h"
#include "value.h"
#include "object.h"
#include "vm.h"

#define OP2STR(instruction) \
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
     (instruction) == OP_ZERO ? "OP_ZERO" : \
     (instruction) == OP_ONE ? "OP_ONE" : \
     (instruction) == OP_NEG_ONE ? "OP_NEG_ONE" : \
     (instruction) == OP_NIL ? "OP_NIL" : \
     (instruction) == OP_CONSTANT ? "OP_CONSTANT" : \
     (instruction) == OP_CONSTANT_LONG ? "OP_CONSTANT_LONG" : \
     (instruction) == OP_PRINT ? "OP_PRINT" : \
     (instruction) == OP_POP ? "OP_POP" : \
     (instruction) == OP_DEFINE_GLOBAL ? "OP_DEFINE_GLOBAL" : \
     (instruction) == OP_ACCESS_GLOBAL ? "OP_ACCESS_GLOBAL" : \
     (instruction) == OP_ACCESS_LOCAL ? "OP_ACCESS_LOCAL" : \
     (instruction) == OP_ACCESS_LOCAL_LONG ? "OP_ACCESS_LOCAL_LONG" : \
     (instruction) == OP_SET_GLOBAL ? "OP_SET_GLOBAL" : \
     (instruction) == OP_SET_LOCAL ? "OP_SET_LOCAL" : \
     (instruction) == OP_SET_LOCAL_LONG ? "OP_SET_LOCAL_LONG" : \
     (instruction) == OP_DEFINE_GLOBAL_LONG ? "OP_DEFINE_GLOBAL_LONG" : \
     (instruction) == OP_SET_GLOBAL_LONG ? "OP_SET_GLOBAL_LONG" : \
     (instruction) == OP_JUMP_IF_FALSE ? "OP_JUMP_IF_FALSE" : \
     (instruction) == OP_JUMP_IF_TRUE ? "OP_JUMP_IF_TRUE" : \
     (instruction) == OP_JUMP ? "OP_JUMP" : \
     (instruction) == OP_LOOP ? "OP_LOOP" : \
     "UNKNOWN_INSTRUCTION")

bool appendNewline = true;

static unsigned int simpleInstruction(const char *name, uint8_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static unsigned int constantInstruction(const char *name, Chunk_t *chunk, uint8_t offset, bool isGlobal) {
    uint8_t constantIdx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIdx);
    /**
     * At this point, each local has its own slot in the VM stack. Each global has its own slot in globalValues.
     * At compile time, equal-valued constants have the same slot in constant pool. This leads to confusion when
     * situations like the below arise:
     *  var a = 2 (const pool idx 0, stack idx 0); var b = 2 (const pool idx 0, stack idx 1);
     * 
     * The original goal was to have a unique index in the const pool for each unique instance of a constant.
     * Now, to save space, we've de-duplicated. This is nice for saving space but tough for debugging.
     * For now, I'm just going to refrain from printing constant pool values as the conflation between constant pool indices,
     * stack slots, and globalValues slots is becoming a little unwieldy...
     * 
     * At least we get to see how the stack evolves over time ? :)
     * 
     * TODO: Add better debugability to be able to see correct constant regardless of de-duplication.
     *       Might just need to add back original duplication code when DEBUG_CHUNK is on...
     */
    // printValue(value);
    printf("'\n");
    return offset + 2;
}

static unsigned jumpInstruction(const char *name, int sign, Chunk_t *chunk, uint8_t offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];

    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static unsigned int longConstantInstruction(const char *name, Chunk_t *chunk, uint8_t offset, bool isGlobal) {
    unsigned constantIdx = 0;
    constantIdx |= chunk->code[offset + 1] << 16;
    constantIdx |= chunk->code[offset + 2] << 8;
    constantIdx |= chunk->code[offset + 3];
    // Value_t value = isGlobal ? vm.globalValues.values[constantIdx] : chunk->constants.values[constantIdx];
    printf("%-16s %4d '", name, constantIdx);
    // printValue(value);
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
    char *name = OP2STR(instruction);
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
        case OP_ZERO:
        case OP_ONE:
        case OP_NEG_ONE:
        case OP_NIL:
        case OP_NOT:
        case OP_PRINT:
        case OP_POP:
            return simpleInstruction(name, offset);
        case OP_CONSTANT:
        case OP_ACCESS_LOCAL:
        case OP_SET_LOCAL:
            return constantInstruction(name, chunk, offset, false);
        case OP_DEFINE_GLOBAL:
        case OP_ACCESS_GLOBAL:
        case OP_SET_GLOBAL:
            return constantInstruction(name, chunk, offset, true);
        case OP_CONSTANT_LONG:
        case OP_ACCESS_LOCAL_LONG:
        case OP_SET_LOCAL_LONG:
            return longConstantInstruction(name, chunk, offset, false);
        case OP_DEFINE_GLOBAL_LONG:
        case OP_ACCESS_GLOBAL_LONG:
        case OP_SET_GLOBAL_LONG:
            return longConstantInstruction(name, chunk, offset, true);
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_JUMP:
            return jumpInstruction(name, 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction(name, -1, chunk, offset);
        default:
            fprintf(stderr, "Unknown opcode %u.\n", instruction);
            return offset + 1;
    }
}

void printObject(Value_t val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            printf("\"%s\"", AS_CSTRING(val));
            if (appendNewline) printf("\n");
            break;
    }
}

void printValue(Value_t val) {
    switch(val.type) {
        case VAL_BOOL:
            printf(AS_BOOL(val) ? "true" : "false");
            if (appendNewline) printf("\n");
            break;
        case VAL_NIL:
            printf("nil");
            if (appendNewline) printf("\n");
            break;
        case VAL_NUM:
            printf("%g", AS_NUMBER(val));
            if (appendNewline) printf("\n");
            break;
        case VAL_OBJ: printObject(val);
    }
}

void disassembleChunk(Chunk_t *chunk, const char *name) {
    printf("==%s==\n", name);
    for (unsigned offset=0; offset<chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}
