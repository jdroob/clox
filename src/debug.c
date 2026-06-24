#include "debug.h"
#include "value.h"
#include "object.h"
#include "vm.h"


#define OP2STR(instruction) \
    ((instruction) == OP_RETURN ? "OP_RETURN" : \
     (instruction) == OP_CALL ? "OP_CALL" : \
     (instruction) == OP_CALL_LONG ? "OP_CALL_LONG" : \
     (instruction) == OP_NEGATE ? "OP_NEGATE" : \
     (instruction) == OP_ADD ? "OP_ADD" : \
     (instruction) == OP_GT ? "OP_GT" : \
     (instruction) == OP_LT ? "OP_LT" : \
     (instruction) == OP_EQ ? "OP_EQ" : \
     (instruction) == OP_NOT ? "OP_NOT" : \
     (instruction) == OP_SUBTRACT ? "OP_SUBTRACT" : \
     (instruction) == OP_MULTIPLY ? "OP_MULTIPLY" : \
     (instruction) == OP_DIVIDE ? "OP_DIVIDE" : \
     (instruction) == OP_MODULO ? "OP_MODULO" : \
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
     (instruction) == OP_DEL ? "OP_DEL" : \
     (instruction) == OP_DEL_LONG ? "OP_DEL_LONG" : \
     (instruction) == OP_DEL_IDCTOR ? "OP_DEL_IDCTOR" : \
     (instruction) == OP_DEFINE_GLOBAL ? "OP_DEFINE_GLOBAL" : \
     (instruction) == OP_ACCESS_GLOBAL ? "OP_ACCESS_GLOBAL" : \
     (instruction) == OP_ACCESS_LOCAL ? "OP_ACCESS_LOCAL" : \
     (instruction) == OP_ACCESS_UPVALUE ? "OP_ACCESS_UPVALUE" : \
     (instruction) == OP_ACCESS_UPVALUE_LONG ? "OP_ACCESS_UPVALUE_LONG" : \
     (instruction) == OP_ACCESS_LOCAL_LONG ? "OP_ACCESS_LOCAL_LONG" : \
     (instruction) == OP_SET_GLOBAL ? "OP_SET_GLOBAL" : \
     (instruction) == OP_SET_LOCAL ? "OP_SET_LOCAL" : \
     (instruction) == OP_SET_LOCAL_LONG ? "OP_SET_LOCAL_LONG" : \
     (instruction) == OP_DEFINE_GLOBAL_LONG ? "OP_DEFINE_GLOBAL_LONG" : \
     (instruction) == OP_SET_GLOBAL_LONG ? "OP_SET_GLOBAL_LONG" : \
     (instruction) == OP_JUMP_IF_NOT_MATCH ? "OP_JUMP_IF_NOT_MATCH" : \
     (instruction) == OP_JUMP_IF_TRUE ? "OP_JUMP_IF_TRUE" : \
     (instruction) == OP_JUMP_IF_FALSE ? "OP_JUMP_IF_FALSE" : \
     (instruction) == OP_JUMP ? "OP_JUMP" : \
     (instruction) == OP_LOOP ? "OP_LOOP" : \
     (instruction) == OP_SWITCH ? "OP_SWITCH" : \
     (instruction) == OP_CASE ? "OP_CASE" : \
     (instruction) == OP_BREAK ? "OP_BREAK" : \
     (instruction) == OP_BREAKALL ? "OP_BREAKALL" : \
     (instruction) == OP_DEFAULTCASE ? "OP_DEFAULTCASE" : \
     (instruction) == OP_ENDSWITCH ? "OP_ENDSWITCH" : \
     (instruction) == OP_CLOSURE ? "OP_CLOSURE" : \
     (instruction) == OP_CLOSURE_LONG ? "OP_CLOSURE_LONG" : \
     (instruction) == OP_METHOD ? "OP_METHOD" : \
     (instruction) == OP_METHOD_LONG ? "OP_METHOD_LONG" : \
     (instruction) == OP_CLOSE_UPVALUE ? "OP_CLOSE_UPVALUE" : \
     (instruction) == OP_CLASS ? "OP_CLASS" : \
     (instruction) == OP_CLASS_LONG ? "OP_CLASS_LONG" : \
     (instruction) == OP_SET_PROPERTY ? "OP_SET_PROPERTY" : \
     (instruction) == OP_SET_PROPERTY_LONG ? "OP_SET_PROPERTY_LONG" : \
     (instruction) == OP_GET_PROPERTY ? "OP_GET_PROPERTY" : \
     (instruction) == OP_GET_PROPERTY_LONG ? "OP_GET_PROPERTY_LONG" : \
     (instruction) == OP_SET_PROPERTY_IDCTOR ? "OP_SET_PROPERTY_IDCTOR" : \
     (instruction) == OP_GET_PROPERTY_IDCTOR ? "OP_GET_PROPERTY_IDCTOR" : \
     "UNKNOWN_INSTRUCTION")

bool appendNewline = true;

static unsigned int simpleInstruction(const char *name, uint32_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static unsigned int constantInstruction(const char *name, Chunk_t *chunk, uint32_t offset, bool isGlobal) {
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

static unsigned jumpInstruction(const char *name, int sign, Chunk_t *chunk, uint32_t offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];

    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static unsigned int longConstantInstruction(const char *name, Chunk_t *chunk, uint32_t offset, bool isGlobal) {
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
        case OP_ADD:
        case OP_GT:
        case OP_LT:
        case OP_EQ:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MODULO:
        case OP_TRUE:
        case OP_FALSE:
        case OP_ZERO:
        case OP_ONE:
        case OP_NEG_ONE:
        case OP_NIL:
        case OP_NOT:
        case OP_PRINT:
        case OP_POP:
        case OP_CLOSE_UPVALUE:
        case OP_SWITCH:
        case OP_CASE:
        case OP_DEFAULTCASE:
        case OP_SET_UPVALUE:
        case OP_GET_PROPERTY_IDCTOR:
        case OP_SET_PROPERTY_IDCTOR:
        case OP_DEL_IDCTOR:
        return simpleInstruction(name, offset);
        case OP_DEL:
        case OP_CONSTANT:
        case OP_ACCESS_LOCAL:
        case OP_ACCESS_UPVALUE:
        case OP_SET_LOCAL:
        case OP_ENDSWITCH:
        case OP_CALL:
        case OP_CLASS:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_METHOD:
            return constantInstruction(name, chunk, offset, false);
        case OP_DEFINE_GLOBAL:
        case OP_ACCESS_GLOBAL:
        case OP_SET_GLOBAL:
            return constantInstruction(name, chunk, offset, true);
        case OP_DEL_LONG:
        case OP_CONSTANT_LONG:
        case OP_CLASS_LONG:
        case OP_ACCESS_LOCAL_LONG:
        case OP_SET_LOCAL_LONG:
        case OP_ACCESS_UPVALUE_LONG:
        case OP_SET_UPVALUE_LONG:
        case OP_BREAK:
        case OP_BREAKALL:
        case OP_CALL_LONG:
        case OP_GET_PROPERTY_LONG:
        case OP_SET_PROPERTY_LONG:
        case OP_METHOD_LONG:
            return longConstantInstruction(name, chunk, offset, false);
        case OP_DEFINE_GLOBAL_LONG:
        case OP_ACCESS_GLOBAL_LONG:
        case OP_SET_GLOBAL_LONG:
            return longConstantInstruction(name, chunk, offset, true);
        case OP_JUMP_IF_NOT_MATCH:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_JUMP:
            return jumpInstruction(name, 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction(name, -1, chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t idxOfConstant = chunk->code[offset++];
            printf("%-16s %4d", "OP_CLOSURE", idxOfConstant);
            printValue(chunk->constants.values[idxOfConstant]);
            printf("\n");

            ObjFunction_t *function = AS_FUNCTION(chunk->constants.values[idxOfConstant]);
            for (int j=0; j<function->upvalueCount; ++j) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d | %s %d\n",
                    offset - 2, isLocal ? "local" : "upvalue",
                    index);
            }

            return offset;
        }
        case OP_CLOSURE_LONG: {
            unsigned idxOfConstant = 0;
            idxOfConstant |= chunk->code[offset + 1] << 16;
            idxOfConstant |= chunk->code[offset + 2] << 8;
            idxOfConstant |= chunk->code[offset + 3];
            printf("%-16s %4d", "OP_CLOSURE", idxOfConstant);
            printValue(chunk->constants.values[idxOfConstant]);
            printf("\n");

            ObjFunction_t *function = AS_FUNCTION(chunk->constants.values[idxOfConstant]);
            for (int j=0; j<function->upvalueCount; ++j) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d | %s %d\n",
                    offset - 2, isLocal ? "local" : "upvalue",
                    index);
            }
            
            return offset + 4;
        }
        default:
            fprintf(stderr, "Unknown opcode %u.\n", instruction);
            return offset + 1;
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
