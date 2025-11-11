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

/**
 * Given a table and a (new) capacity:
 *  (i)   Allocate a new table of size capacity
 *  (ii)  Initialize key-value pairs in table to NULL / NIL_VAL
 *  (iii) Walk original table. For each non-NULL key, 
 *      (iii.1) Determine where in new table, key-value pair should be stored
 *      (iii.2) Store new key-value pair
 * 
 *  (iv) Free original table
 *  (v)  Set table's entries to new entries table  
 */
static void adjustCapacity(Table_t *table, int capacity) {
    // 1. Allocate capacity entries and set to NULL
    // 2. Fill in empty array
    Entry_t *entries = ALLOCATE(Entry_t, capacity);
    for (int i=0; i<capacity; ++i) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    for (int i=0; i<capacity; ++i) {
        Entry_t *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry_t *dest = findEntry(entries, capacity, entry->key);   // find new bucket in new array
        dest->key = entry->key;
        dest->value = entry->value;
    }

    FREE_ARRAY(Entry_t, table->entries, table->capacity);
    table->entries = entries;
}

/**
 * Given a table, a key, and a value:
 * (i)  Determine if the key is already present in table. 
 *       If not, increment table's count.
 * (ii) findEntry returns a pointer to location in table where 
 *        new key-value pair should be stored (i.e. findEntry is where linear probing happens).
 * (iii) Using the result of findEntry, store the key-value pair.
 * (iv) Return the boolean result of whether the key is new.
 */
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

/**
 * Copy all entries from src to dst.
 * Entries in dst will be placed in appropriate bucket
 * based on dst's hash (i.e. dst's layout not guaranteed to match src's)
 */
void tableAddAll(Table_t *src, Table_t *dst) {
    for (int i=0; i<src->capacity; ++i) {
        Entry_t *entry = &src[i];
        if (entry->key == NULL) continue;
        setTable(dst, entry->key, entry->value);
    }
}
