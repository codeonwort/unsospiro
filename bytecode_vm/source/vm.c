#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
// For native functions
#include <time.h>
#include <stdlib.h>

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

	// Print callstack (last to first)
	for (int i = vm->frameCount - 1; i >= 0; --i) {
		CallFrame* frame = &(vm->frames[i]);
		ObjFunction* function = frame->closure->function;
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf_s(stderr, "[line %d] in ", function->chunk.lines[instruction]);
		if (function->name == NULL) {
			fprintf_s(stderr, "screipt\n");
		} else {
			fprintf_s(stderr, "%s()\n", function->name->chars);
		}
	}

	resetStack(vm);
}

static void defineNative(VM* vm, const char* name, NativeFn function) {
	// Push name and function to the stack to prevent from being GC'd.
	push(vm, OBJ_VAL(copyString(vm, name, (int)strlen(name))));
	push(vm, OBJ_VAL(newNative(vm, function)));
	tableSet(&(vm->globals), AS_STRING(vm->stack[0]), vm->stack[1]);
	pop(vm);
	pop(vm);
}

static Value peek(VM* vm, int distance) {
	return vm->stackTop[-1 - distance];
}

static bool call(VM* vm, ObjClosure* closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimeError(vm, "Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}
	if (vm->frameCount == FRAMES_MAX) {
		runtimeError(vm, "Stack overflow.");
		return false;
	}
	CallFrame* frame = &(vm->frames[vm->frameCount++]);
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm->stackTop - argCount - 1;
	return true;
}

static bool callValue(VM* vm, Value callee, int argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
			case OBJ_CLOSURE:
				return call(vm, AS_CLOSURE(callee), argCount);
			case OBJ_NATIVE: {
				NativeFn native = AS_NATIVE(callee);
				// #todo: Check arity
				// #todo: Throw runtime error if any
				Value result = native(vm, argCount, vm->stackTop - argCount);
				// #todo: How do I determine if a native function called runtimeError()?
				// runtimeError() invokes resetStack() so let's check vm->frameCount for now.
				if (vm->frameCount == 0) {
					return false;
				} else {
					vm->stackTop -= argCount + 1;
					push(vm, result);
					return true;
				}
			}
			default:
				break; // Not callable object type
		}
	}
	runtimeError(vm, "Can only call functions and classes.");
	return false;
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
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
		disassembleInstruction(&(frame->closure->function->chunk), (int)(frame->ip - frame->closure->function->chunk.code));
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
			case OP_CALL: {
				int argCount = READ_BYTE();
				if (!callValue(vm, peek(vm, argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &(vm->frames[vm->frameCount - 1]);
				break;
			}
			case OP_CLOSURE: {
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
				ObjClosure* closure = newClosure(vm, function);
				push(vm, OBJ_VAL(closure));
				break;
			}
			case OP_RETURN: {
				Value result = pop(vm);
				vm->frameCount--;
				if (vm->frameCount == 0) {
					pop(vm);
					return INTERPRET_OK;
				}
				vm->stackTop = frame->slots;
				push(vm, result);
				frame = &(vm->frames[vm->frameCount - 1]);
				break;
			}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

static Value clockNative(VM* vm, int argCount, Value* args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value readFileNative(VM* vm, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "[readFileNative] Invalid number of arguments: 1 was expected, but %d was given", argCount);
		return NIL_VAL;
	}
	if (IS_STRING(args[0]) == false) {
		runtimeError(vm, "[readFileNative] The argument is not a string", argCount);
		return NIL_VAL;
	}

	char* filepath = AS_CSTRING(args[0]);
	FILE* fp;
	fopen_s(&fp, filepath, "rb");
	if (!fp) {
		runtimeError(vm, "[readFileNative] Failed to open file: %s", filepath);
		return NIL_VAL;
	}

	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	rewind(fp);

	if (fsize > INT_MAX) {
		runtimeError(vm, "[readFileNative] File is too big: %s (%llu bytes)", filepath, fsize);
		fclose(fp);
		return NIL_VAL;
	}
	
	char* contents = malloc(fsize + 1);
	if (contents == NULL) {
		runtimeError(vm, "[readFileNative] Out of memory while reading file: %s", filepath);
		fclose(fp);
		return NIL_VAL;
	}

	size_t bytesRead = fread(contents, sizeof(char), fsize, fp);
	if (bytesRead < fsize) {
		runtimeError(vm, "[readFileNative] Failed to read file: %s", filepath);
		fclose(fp);
		free(contents);
		return NIL_VAL;
	}
	contents[fsize] = '\0';

	// #todo-native: 1) Redundant malloc. 2) VM needs to hash the string.
	ObjString* string = copyString(vm, contents, (int)fsize);
	fclose(fp);
	free(contents);

	return OBJ_VAL(string);
}

void initVM(VM* vm) {
	resetStack(vm);
	vm->objects = NULL;
	initTable(&vm->globals);
	initTable(&vm->strings);

	defineNative(vm, "clock", clockNative);
	defineNative(vm, "readFile", readFileNative);
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
	ObjClosure* closure = newClosure(vm, function);
	pop(vm);
	push(vm, OBJ_VAL(closure));
	call(vm, closure, 0);

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
