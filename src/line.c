#include "line.h"
#include "memory.h"


void initLineMap(LineMap_t *lines) {
    lines->capacity = 0;
    lines->count = 0;
    lines->encoding = NULL;
}

static void writeLine(LineMap_t *lines, int newLine) {
    if (lines->count > 0) {
        int currLine = lines->encoding[lines->count - 1].line;
        if (currLine == newLine) {
            lines->encoding[lines->count - 1].frequency++;
            return;
        }
    }
    lines->encoding[lines->count].line = newLine;
    lines->encoding[lines->count++].frequency = 1;
}

void writeLineMap(LineMap_t *lines, int line) {
    if (lines->capacity < lines->count + 1) {
        size_t oldCapacity = lines->capacity;
        lines->capacity = GROW_CAPACITY(oldCapacity);
        lines->encoding = GROW_ARRAY(Pair_t, lines->encoding, oldCapacity, lines->capacity);
    }
    writeLine(lines, line);
}

void freeLineMap(LineMap_t *lines) {
    FREE_ARRAY(Pair_t, lines->encoding, lines->capacity);
    initLineMap(lines);
}
