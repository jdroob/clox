#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
    ObjString_t *key;
    Value_t value;
} Entry_t;

typedef struct {
    int count;
    int capacity;
    Entry_t *entries;
} Table_t;

void initTable(Table_t *table);
void freeTable(Table_t *table);
bool setTable(Table_t *table, ObjString_t *key, Value_t value);

#endif // CLOX_TABLE_H