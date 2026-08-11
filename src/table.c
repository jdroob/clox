#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "debug.h"
#include "vm.h"


#define TABLE_MAX_LOAD 0.75

void initTable(Table_t *table) {
    table->count = table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table_t *table) {
    FREE_ARRAY(Entry_t, table->entries, table->capacity);
    initTable(table);
}

uint32_t hashDouble(double value) {
    union bitCast { // 8-byte union
        double value;
        uint32_t ints[2];
    };

    union bitCast cast;
    cast.value = value + 1.0;
    return cast.ints[0] + cast.ints[1];
}

static uint32_t getHash(Value_t key) {
    switch (key.type) {
        case VAL_BOOL: return key.as.boolean ? 3 : 5;
        case VAL_EMPTY:
        case VAL_NIL:  return 7;
        case VAL_NUM:  return hashDouble(AS_NUMBER(key));
        case VAL_OBJ: {
            Obj_t *obj = AS_OBJ(key);
            switch (obj->type) {
                case OBJ_STRING: {
                    ObjString_t *string = (ObjString_t *)obj;
                    return string->hash;
                }
            }
        }
        default:
            fprintf(stderr, "Error! Invalid key\n");
            exit(EXIT_FAILURE);
    }
}

static Entry_t *findEntry(Entry_t *entries, int capacity, Value_t key) {
    /**
     * a % b  == a & (b - 1) iff b is power of two (which it is for us)
     * 
     * e.g. 229 mod 64 = 37
     * 
     * 229:   1110_0101
     * % 64:  0100_0000
     * _________________
     * 37:    0010_0101
     * 
     * 229:   1110_0101
     * & 63:  0011_1111
     * _________________
     * 37:    0010_0101
     */
    // uint32_t index = getHash(key) % capacity;
    uint32_t index = getHash(key) & (capacity - 1);
    Entry_t *tombstone = NULL;
    for (;;) {
        Entry_t *entry = &entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value)) {
                // Empty entry
                /**
                 * If tombstone is not NULL, we've already set tombstone 
                 * at some point during linear probing.
                 * (i.e. we found an entry that's been deleted in the middle 
                 *       of a probing sequence)
                 */
                return tombstone != NULL ? tombstone : entry;
            } else {
                // Found a tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        } else {
            if (valuesEqual(key, entry->key)) return entry;
        }
        index = (index + 1) % capacity;
    }
}

/**
 * Given a table and a (new) capacity:
 *  (i)   Allocate a new entries array of size capacity
 *  (ii)  Initialize key-value pairs in array to NULL / NIL_VAL
 *  (iii) Walk original entries array. For each non-NULL key, 
 *      (iii.1) Determine where in new array, key-value pair should be stored
 *      (iii.2) Store new key-value pair
 * 
 *  (iv) Free original array
 *  (v)  Set table's entries field to new entries array  
 */
static void adjustCapacity(Table_t *table, int capacity) {
    Entry_t *entries = ALLOCATE(Entry_t, capacity);
    for (int i=0; i<capacity; ++i) {
        entries[i].key = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;   // reset count b/c we're not counting tombstones from prev
    if (table->capacity != 0) {
        for (int i=0; i<table->capacity; ++i) {
            Entry_t *entry = &table->entries[i];
            if (IS_EMPTY(entry->key)) continue;

            Entry_t *dest = findEntry(entries, capacity, entry->key);   // find new bucket in new array
            dest->key = entry->key;
            dest->value = entry->value;
            table->count++;
        }
        FREE_ARRAY(Entry_t, table->entries, table->capacity);
    }

    table->entries = entries;
    table->capacity = capacity;
}

/**
 * Given a table, a key, and a value:
 * (i)   Determine if the key is already present in table. 
 *        If not, increment table's count.
 * (ii)  findEntry returns a pointer to location in table where 
 *        new key-value pair should be stored (findEntry is where linear probing actually happens).
 * (iii) Using the result of findEntry, store the key-value pair.
 * (iv)  Return the boolean result of whether the key is new.
 */
bool tableSet(Table_t *table, Value_t key, Value_t value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry_t *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = IS_EMPTY(entry->key);

    // Don't increment if entry is a tombstone
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    // inserting new key-value pair into array
    entry->key = key;
    entry->value = value;

    return isNewKey;
}

/**
 * Given a key, return the corresponding value if the key exists in table.
 * value is an output parameter.
 * Return true if the value is found.
 */
bool tableGet(Table_t *table, Value_t key, Value_t *value) {
    if (table->count == 0) return false;

    Entry_t *entry = findEntry(table->entries, table->capacity, key);
    if (IS_NIL(entry->key) || IS_EMPTY(entry->key)) return false;

    *value = entry->value;
    return true;
}

bool tableDelete(Table_t *table, Value_t key) {
    if (table->count == 0) return false;

    Entry_t *entry = findEntry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) return false;   // Did not find entry to delete

    // Tombstone
    entry->key = EMPTY_VAL;
    entry->value = BOOL_VAL(true);

    // table->count--;  // Treat tombstones as pseudo-occupied entries to avoid infinite loop during probing sequences
    return true;
}

/**
 * Copy all entries from src to dst.
 * Entries in dst will be placed in appropriate bucket
 * based on entry's hash & dst's capacity (i.e. dst's layout not guaranteed to match src's)
 */
void tableAddAll(Table_t *src, Table_t *dst) {
    for (int i=0; i<src->capacity; ++i) {
        Entry_t *entry = &src->entries[i];
        if (IS_EMPTY(entry->key)) continue;
        tableSet(dst, entry->key, entry->value);
    }
}

void printTable(Table_t *tbl) {
    appendNewline = false;
    printf("{\n");
    for (int i=0; i<tbl->capacity; ++i) {
        Entry_t *entry = &tbl->entries[i];
        if (IS_EMPTY(entry->key)) continue;
        printf("\t{ ");
        printValue(entry->key);
        printf(" : ");
        printValue(entry->value); 
        printf(" },\n");
    }
    printf("}");
    appendNewline = true;
}

ObjString_t *tableFindString(Table_t *table, const char *chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    Entry_t *entry;
    /**
     * a % b  == a & (b - 1) iff b is power of two (which it is for us)
     */
    // int index = hash % table->capacity;
    int index = hash & (table->capacity - 1);
    for (;;) {
        entry = &table->entries[index];
        // Found non-tombstone empty entry
        if (IS_EMPTY(entry->key) && IS_NIL(entry->value)) return NULL;
        
        if (IS_STRING(entry->key)) {    // Avoid collisions b/w different types
            ObjString_t *string = AS_STRING(entry->key);
            if (string->length == length &&
                string->hash == hash &&
                !memcmp(string->chars, chars, length)) {
                    // ObjString_t *string = AS_STRING(entry->key);
                    return string;
                }
        }
        index = (index + 1) % table->capacity;
    }
}

void markTable(Table_t *table) { 
    for (size_t i=0; i<table->capacity; ++i) {
        Entry_t *entry = &table->entries[i];
        markObject(AS_OBJ(entry->key));  // mark ObjString_t (name of var)
        unsigned idx = (unsigned)AS_NUMBER(entry->value);
        if (vm.globalValues.values != NULL) {
            markValue(getValueAt(&vm.globalValues, idx));  // for vm startup edge case
        }
    }
}

void tableRemoveWhite(Table_t *table) {
    for (size_t i=0; i<table->capacity; ++i) {
        Entry_t *entry = &table->entries[i];
        if (AS_OBJ(entry->key) != NULL && 
            !IS_MARKED(entry->key) &&
            !IS_PROTECTED(entry->key)) {
            tableDelete(table, entry->key);   // ensure no dangling pointers to interned strings
        }
    }
}
