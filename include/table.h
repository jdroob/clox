#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
    Value_t key;
    Value_t value;
} Entry_t;

typedef struct {
    int count;
    int capacity;
    Entry_t *entries;
} Table_t;

void initTable(Table_t *table);
void freeTable(Table_t *table);
void tableAddAll(Table_t *src, Table_t *dst);
bool tableSet(Table_t *table, Value_t key, Value_t value);
bool tableDelete(Table_t *table, Value_t key);
bool tableGet(Table_t *table, Value_t key, Value_t *value);
ObjString_t *tableFindString(Table_t *table, const char *chars, int length, uint32_t hash);
void markTable(Table_t *table);
void tableRemoveWhite(Table_t *table);
void printTable(Table_t *table);

#endif // CLOX_TABLE_H