#pragma once

#include "common.h"

#define ALLOCATE(type, count) \
	(type*)reallocate(NULL, 0, sizeof(type) * (count))

// #todo: Initial size = 8 and growth multiplier = 2. Profile and find optimal parameters.
#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity * 2))

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
	(type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
	reallocate(pointer, sizeof(type) * (oldCount), 0)

// This function will be globally used for dynamic memory management.
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
