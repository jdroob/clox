#ifndef CLOX_LINE_H
#define CLOX_LINE_H

#include "common.h"

typedef struct {
    /**
     * line -> #bytecode instrs
     */
    int line;
    size_t frequency;
} Pair_t;

typedef struct {
    size_t count;
    size_t capacity;
    Pair_t *encoding;
} LineMap_t;

void initLineMap(LineMap_t *lines);
void freeLineMap(LineMap_t *lines);
void writeLineMap(LineMap_t *lines, int line);

#endif // CLOX_LINE_H
