#pragma once

#include "common.h"
#include "value.h"

typedef struct VM_t VM;

typedef struct {
	ObjString* key;
	Value value;
} Entry;

typedef struct {
	int count;    // Entry count + Tombstone count.
	int capacity; // Max size. (load Factor = count / capacity)
	Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(VM* vm, Table* table);
void markTable(VM* vm, Table* table);
