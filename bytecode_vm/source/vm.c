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
	vm->frameCount = 0;
}

static void runtimeError(VM* vm, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf_s(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	CallFrame* frame = &(vm->frames[vm->frameCount - 1]);
	size_t instruction = frame->ip - frame->function->chunk.code - 1;
	int line = frame->function->chunk.lines[instruction];
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
	CallFrame* frame = &(vm->frames[vm->frameCount - 1]);

#define READ_BYTE() (*(frame->ip++))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
		disassembleInstruction(&(frame->function->chunk), (int)(frame->ip - frame->function->chunk->code));
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
			case OP_POP: pop(vm); break;
			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(vm, frame->slots[slot]);
				break;
			}
			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(vm, 0);
				break;
			}
			case OP_GET_GLOBAL: {
				ObjString* name = READ_STRING();
				Value value;
				// #todo: Accessing hash table every time is slow.
				if (!tableGet(&vm->globals, name, &value)) {
					// #todo: If executing from file, this could be reported at compile-time.
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(vm, value);
				break;
			}
			case OP_DEFINE_GLOBAL: {
				ObjString* name = READ_STRING();
				tableSet(&vm->globals, name, peek(vm, 0));
				pop(vm);
				break;
			}
			case OP_SET_GLOBAL: {
				ObjString* name = READ_STRING();
				// #todo: If undefined variable, add then remove an entry to no avail.
				if (tableSet(&vm->globals, name, peek(vm, 0))) {
					tableDelete(&vm->globals, name);
					runtimeError(vm, "Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER: BINARY_OP(vm, BOOL_VAL, >); break;
			case OP_LESS: BINARY_OP(vm, BOOL_VAL, <); break;
			case OP_ADD: {
				// OP_ADD's stack effect is -1. (pop 2, push 1)
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
			case OP_PRINT: {
				// OP_PRINT's stack effect is zero.
				printValue(pop(vm));
				printf("\n");
				break;
			}
			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsey(peek(vm, 0))) frame->ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}
			case OP_RETURN: {
				return INTERPRET_OK; // #todo: No function yet; just exit for now.
			}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

void initVM(VM* vm) {
	resetStack(vm);
	vm->objects = NULL;
	initTable(&vm->globals);
	initTable(&vm->strings);
}

void freeVM(VM* vm) {
	freeTable(&vm->globals);
	freeTable(&vm->strings);
	freeObjects(vm);
}

InterpretResult interpret(VM* vm, const char* source) {
	ObjFunction* function = compile(vm, source);
	if (function == NULL) return INTERPRET_COMPILE_ERROR;

	push(vm, OBJ_VAL(function));
	CallFrame* frame = &(vm->frames[vm->frameCount++]);
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm->stack;

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
