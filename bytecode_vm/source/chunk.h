#pragma once

#include "common.h"

// Each operation is represented by one-byte opcode.
typedef enum {
	OP_RETURN,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
} Chunk;
