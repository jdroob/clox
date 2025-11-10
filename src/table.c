#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table_t *table) {
    table->count = table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table_t *table) {
    FREE_ARRAY(Entry_t, table->entries, table->capacity);
    initTable(table);
}

static Entry_t *findEntry(Entry_t *entries, int capacity, ObjString_t *key) {
    uint32_t index = key->hash % capacity;
    for (;;) {
        Entry_t *entry = &entries[index];
        if (entry->key == key || entry->key == NULL) return entry;
        index = (index + 1) % capacity;
    }
}

void adjustCapacity(Table_t *table, int capacity) {
    table->entries = GROW_ARRAY(Entry_t, table->entries, table->capacity, capacity);
    table->capacity = capacity;
    for (int i=table->count; i<table->capacity; ++i) {
        table->entries[i].key = NULL;
    }
}

bool setTable(Table_t *table, ObjString_t *key, Value_t value) {
    if (table->capacity + 1 < table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry_t *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    if (isNewKey) table->count++;

    // inserting new key-value pair into array
    entry->key = key;
    entry->value = value;

    return isNewKey;
}


