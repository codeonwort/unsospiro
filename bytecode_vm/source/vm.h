#pragma once

#include "chunk.h"
#include "value.h"

// #todo: Temp stack size
#define STACK_MAX 256

typedef struct {
	Chunk* chunk;
	uint8_t* ip; // instruction pointer (or program counter) to next instruction to be executed
	Value stack[STACK_MAX];
	Value* stackTop; // location where in next value will be pushed
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, Chunk* chunk);
void push(VM* vm, Value value);
Value pop(VM* vm);
