#pragma once

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

// #todo: Temp stack size
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
	ObjClosure* closure;
	uint8_t* ip; // instruction pointer (or program counter) to next instruction to be executed
	Value* slots;
} CallFrame;

typedef struct VM_t {
	CallFrame frames[FRAMES_MAX];
	int frameCount;

	Value stack[STACK_MAX];
	Value* stackTop; // location where in next value will be pushed
	Table globals; // Store global variables
	Table strings; // Store all strings in a hash table for string interning
	ObjUpvalue* openUpvalues;

	size_t bytesAllocated;
	size_t nextGC; // Threshold to trigger GC
	Obj* objects;

	// For GC
	int grayCount;
	int grayCapacity;
	Obj** grayStack;
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

// #todo-gc: Temp var for GC. Don't use for other purpose.
extern VM* g_vm;
