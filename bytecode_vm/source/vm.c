#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void resetStack(VM* vm) {
	vm->stackTop = vm->stack;
}

static void runtimeError(VM* vm, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf_s(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm->ip - vm->chunk->code - 1;
	int line = vm->chunk->lines[instruction];
	fprintf_s(stderr, "[line %d] in script\n", line);
	resetStack(vm);
}

static Value peek(VM* vm, int distance) {
	return vm->stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
	// #todo: Number 0 is falsey
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0.0);
}

static void concatenate(VM* vm) {
	ObjString* b = AS_STRING(pop(vm));
	ObjString* a = AS_STRING(pop(vm));

	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy_s(chars, a->length, a->chars, a->length);
	memcpy_s(chars + a->length, b->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(vm, chars, length);
	push(vm, OBJ_VAL(result));
}

static InterpretResult run(VM* vm) {
#define READ_BYTE() (*(vm->ip++))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define BINARY_OP(vm, valueType, op) \
	do { \
		if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
			runtimeError(vm, "Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(pop(vm)); \
		double a = AS_NUMBER(pop(vm)); \
		push(vm, valueType(a op b)); \
	} while (false)

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
			case OP_NIL: push(vm, NIL_VAL); break;
			case OP_TRUE: push(vm, BOOL_VAL(true)); break;
			case OP_FALSE: push(vm, BOOL_VAL(false)); break;
			case OP_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER: BINARY_OP(vm, BOOL_VAL, >); break;
			case OP_LESS: BINARY_OP(vm, BOOL_VAL, <); break;
			case OP_ADD: {
				if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
					concatenate(vm);
				} else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
					double b = AS_NUMBER(pop(vm));
					double a = AS_NUMBER(pop(vm));
					push(vm, NUMBER_VAL(a + b));
				} else {
					runtimeError(vm, "Operands must be two numbers or two strings.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT: BINARY_OP(vm, NUMBER_VAL, -); break;
			case OP_MULTIPLY: BINARY_OP(vm, NUMBER_VAL, *); break;
			case OP_DIVIDE: BINARY_OP(vm, NUMBER_VAL, /); break;
			case OP_NOT: push(vm, BOOL_VAL(isFalsey(pop(vm)))); break;
			case OP_NEGATE: {
				if (!IS_NUMBER(peek(vm, 0))) {
					runtimeError(vm, "Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
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
#undef BINARY_OP
}

void initVM(VM* vm) {
	resetStack(vm);
	vm->objects = NULL;
	initTable(&vm->strings);
}

void freeVM(VM* vm) {
	freeTable(&vm->strings);
	freeObjects(vm);
}

InterpretResult interpret(VM* vm, const char* source) {
	Chunk chunk;
	initChunk(&chunk);

	if (!compile(vm, source, &chunk)) {
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm->chunk = &chunk;
	vm->ip = vm->chunk->code;

	InterpretResult result = run(vm);

	freeChunk(&chunk);
	return result;
}

void push(VM* vm, Value value) {
	*(vm->stackTop) = value;
	vm->stackTop++;
}

Value pop(VM* vm) {
	vm->stackTop--;
	return *(vm->stackTop);
}
