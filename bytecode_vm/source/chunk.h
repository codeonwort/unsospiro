#pragma once

#include "common.h"
#include "value.h"

// Each operation is represented by one-byte opcode.
typedef enum {
	OP_CONSTANT,
	OP_NEGATE,
	OP_RETURN,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	int* lines; // Store line number.
	ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value); // Returns linear index of the constant in a constant array.
