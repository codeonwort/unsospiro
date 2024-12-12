#pragma once

#include "chunk.h"
#include "table.h"
#include "value.h"

// #todo: Temp stack size
#define STACK_MAX 256

typedef struct {
	Chunk* chunk;
	uint8_t* ip; // instruction pointer (or program counter) to next instruction to be executed
	Value stack[STACK_MAX];
	Value* stackTop; // location where in next value will be pushed
	Table strings; // Store all strings in a hash table for string interning
	Obj* objects;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);
