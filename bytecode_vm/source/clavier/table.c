#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#include <stdlib.h>
#include <string.h>

// #todo: Optimal load factor
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table* table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	uint32_t index = key->hash % capacity;
	Entry* tombstone = NULL;

	// NOTE: adjustCapacity() guarantees no infinite loop.
	for (;;) {
		Entry* entry = &entries[index];
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			} else {
				if (tombstone == NULL) tombstone = entry;
			}
		} else if (entry->key == key) {
			return entry;
		}

		// Linear probing
		index = (index + 1) % capacity;
	}
}

bool tableGet(Table* table, ObjString* key, Value* value) {
	if (table->count == 0) return false;

	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	*value = entry->value;
	return true;
}

static void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; ++i) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	// We don't need to copy tombstones, so zero it and only calculate entries.
	table->count = 0;

	for (int i = 0; i < table->capacity; ++i) {
		Entry* entry = &table->entries[i];
		if (entry->key == NULL) continue;

		Entry* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++; // Increase only if real entry.
	}

	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	// Do not increase count if a tombstone is being filled.
	if (isNewKey && IS_NIL(entry->value)) table->count++;

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
	// Tombstone trick: Don't actually delete the entry, just replace it with a special marker.
	// When performing probing, if an entry is actually deleted, the probe sequence is broken.

	if (table->count == 0) return false;

	// Find the entry.
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// Put a tombstone to the entry.
	// NOTE: Tombstone still contributes to table->count, so does not decrease it.
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

void tableAddAll(Table* from, Table* to) {
	for (int i = 0; i < from->capacity; ++i) {
		Entry* entry = &from->entries[i];
		if (entry->key != NULL) {
			tableSet(to, entry->key, entry->value);
		}
	}
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
	if (table->count == 0) return NULL;

	uint32_t index = hash % table->capacity;
	for (;;) {
		Entry* entry = &table->entries[index];
		if (entry->key == NULL) {
			// Stop if non-tombstone
			if (IS_NIL(entry->value)) return NULL;
		} else if (entry->key->length == length && entry->key->hash == hash && 0 == memcmp(entry->key->chars, chars, length)) {
			return entry->key;
		}
		index = (index + 1) % table->capacity;
	}
}

void tableRemoveWhite(VM* vm, Table* table) {
	for (int i = 0; i < table->capacity; ++i) {
		Entry* entry = &(table->entries[i]);
		if (entry->key != NULL && !(entry->key->obj.isMarked)) {
			tableDelete(table, entry->key);
		}
	}
}

void markTable(VM* vm, Table* table) {
	for (int i = 0; i < table->capacity; ++i) {
		Entry* entry = &(table->entries[i]);
		markObject(vm, (Obj*)entry->key);
		markValue(vm, entry->value);
	}
}
