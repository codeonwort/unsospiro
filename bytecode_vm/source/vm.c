#include "vm.h"
#include "common.h"
#include "debug.h"

#include <stdio.h>

static void resetStack(VM* vm) {
	vm->stackTop = vm->stack;
}

static InterpretResult run(VM* vm) {
#define READ_BYTE() (*(vm->ip++))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

	// Most performance-critical section in the entire VM.
	for (;;) {
#if DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value* slot = vm->stack; slot < vm->stackTop; ++slot) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

		uint8_t instruction;

		// #todo: Efficient bytecode dispatch. Search for:
		// - direct threaded code
		// - jump table
		// - computed goto
		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(vm, constant);
				break;
			}
			case OP_NEGATE: {
				push(vm, -pop(vm));
				break;
			}
			case OP_RETURN: {
				printValue(pop(vm));
				printf("\n");
				return INTERPRET_OK; // #todo: No function yet; just exit for now.
			}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

void initVM(VM* vm) {
	resetStack(vm);
}

void freeVM(VM* vm) {
	//
}

InterpretResult interpret(VM* vm, Chunk* chunk) {
	vm->chunk = chunk;
	vm->ip = vm->chunk->code;
	return run(vm);
}

void push(VM* vm, Value value) {
	*(vm->stackTop) = value;
	vm->stackTop++;
}

Value pop(VM* vm) {
	vm->stackTop--;
	return *(vm->stackTop);
}
