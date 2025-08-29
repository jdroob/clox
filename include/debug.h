#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "common.h"
#include "chunk.h"

void disassembleChunk(Chunk_t *chunk, const char *name);
unsigned int disassembleInstruction(Chunk_t *chunk, unsigned int offset);
void printValue(Value_t val);

#endif // CLOX_DEBUG_H