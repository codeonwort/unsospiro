#pragma once

#include "chunk.h"

typedef struct {
	Chunk* chunk;
	uint8_t* ip; // instruction pointer (or program counter) to next instruction to be executed
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(VM* vm, Chunk* chunk);
