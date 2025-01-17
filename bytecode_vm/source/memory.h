#pragma once

#include "common.h"
#include "object.h"

typedef struct VM_t VM;

#define ALLOCATE(type, count) \
	(type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

// #todo: Initial size = 8 and growth multiplier = 2. Profile and find optimal parameters.
#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity * 2))

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
	(type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
	reallocate(pointer, sizeof(type) * (oldCount), 0)

// This function will be globally used for dynamic memory management.
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
void collectGarbage(VM* vm);

void freeObjects(VM* vm);
