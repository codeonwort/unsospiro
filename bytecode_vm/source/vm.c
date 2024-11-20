#include "vm.h"
#include "common.h"
#include "debug.h"

#include <stdio.h>

static InterpretResult run(VM* vm) {
#define READ_BYTE() (*(vm->ip++))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

	// Most performance-critical section in the entire VM.
	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
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
				printValue(constant);
				printf("\n");
				break;
			}
			case OP_RETURN: {
				return INTERPRET_OK; // #todo: No function yet; just exit for now.
			}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

void initVM(VM* vm) {
	//
}

void freeVM(VM* vm) {
	//
}

InterpretResult interpret(VM* vm, Chunk* chunk) {
	vm->chunk = chunk;
	vm->ip = vm->chunk->code;
	return run(vm);
}
