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

VM* g_vm = NULL;

static void resetStack(VM* vm) {
	vm->stackTop = vm->stack;
	vm->frameCount = 0;
	vm->openUpvalues = NULL;
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
			case OBJ_BOUND_METHOD: {
				ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
				vm->stackTop[-argCount - 1] = bound->receiver;
				return call(vm, bound->method, argCount);
			}
			case OBJ_CLASS: {
				ObjClass* klass = AS_CLASS(callee);
				vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm, klass));
				Value initializer;
				// #todo: Finding initializer is O(1) but slow
				if (tableGet(&klass->methods, vm->initString, &initializer)) {
					// Call initializer (constructor) if exist.
					return call(vm, AS_CLOSURE(initializer), argCount);
				} else if (argCount != 0) {
					// Passing initializer arguments is invalid if initializer does not exist.
					runtimeError(vm, "Expected 0 arguments but got %d.", argCount);
					return false;
				}
				return true;
			}
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

static bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
	Value method;
	if (!tableGet(&klass->methods, name, &method)) {
		runtimeError(vm, "Undefined property '%s'", name->chars);
		return false;
	}
	return call(vm, AS_CLOSURE(method), argCount);
}

static bool invoke(VM* vm, ObjString* name, int argCount) {
	Value receiver = peek(vm, argCount);

	if (!IS_INSTANCE(receiver)) {
		runtimeError(vm, "Only instances have methods.");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm->stackTop[-argCount - 1] = value;
		return callValue(vm, value, argCount);
	}

	return invokeFromClass(vm, instance->klass, name, argCount);
}

static bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
	Value method;
	if (!tableGet(&klass->methods, name, &method)) {
		runtimeError(vm, "Undefined property '%s'.", name->chars);
		return false;
	}

	ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));

	pop(vm);
	push(vm, OBJ_VAL(bound));
	return true;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm->openUpvalues;
	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjUpvalue* createdUpvalue = newUpvalue(vm, local);
	createdUpvalue->next = upvalue;

	if (prevUpvalue == NULL) {
		vm->openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
	while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
		ObjUpvalue* upvalue = vm->openUpvalues;
		upvalue->closed = *(upvalue->location);
		upvalue->location = &(upvalue->closed);
		vm->openUpvalues = upvalue->next;
	}
}

static void defineMethod(VM* vm, ObjString* name) {
	Value method = peek(vm, 0);
	ObjClass* klass = AS_CLASS(peek(vm, 1));
	tableSet(&(klass->methods), name, method);
	pop(vm);
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

		// Our VM believes that instructions are valid.
		// Other VMs like JVM support execution of pre-compiled bytecode.
		// Such code could be malicious, so JVM validates it first.

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
			case OP_GET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				push(vm, *(frame->closure->upvalues[slot]->location));
				break;
			}
			case OP_SET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				*(frame->closure->upvalues[slot]->location) = peek(vm, 0);
				break;
			}
			// When interpreter hits this instruction, left expression of dot was already executed
			// and the instance is at the top of the stack.
			case OP_GET_PROPERTY: {
				// #todo: User has no way to check if a property exists.
				// #todo: Support obj["accessor"] ?
				if (!IS_INSTANCE(peek(vm, 0))) {
					runtimeError(vm, "Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
				ObjString* name = READ_STRING();

				Value value;
				// #todo: Accessing by name is slow.
				if (tableGet(&(instance->fields), name, &value)) {
					pop(vm); // instance
					push(vm, value);
					break;
				}

				if (!bindMethod(vm, instance->klass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			// #todo: Allow removing fields?
			case OP_SET_PROPERTY: {
				if (!IS_INSTANCE(peek(vm, 1))) {
					runtimeError(vm, "Only instances have fields.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
				tableSet(&(instance->fields), READ_STRING(), peek(vm, 0));
				// Remove instance from stack. (pop value, pop instance, then push value)
				Value value = pop(vm);
				pop(vm);
				push(vm, value);
				break;
			}
			case OP_GET_SUPER: {
				ObjString* name = READ_STRING();
				ObjClass* superclass = AS_CLASS(pop(vm));

				if (!bindMethod(vm, superclass, name)) {
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
			case OP_INVOKE: {
				ObjString* method = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(vm, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm->frames[vm->frameCount - 1];
				break;
			}
			case OP_CLOSURE: {
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
				ObjClosure* closure = newClosure(vm, function);
				push(vm, OBJ_VAL(closure));
				for (int i = 0; i < closure->upvalueCount; ++i) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();
					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}
				break;
			}
			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm, vm->stackTop - 1);
				pop(vm);
				break;
			}
			case OP_RETURN: {
				Value result = pop(vm);
				closeUpvalues(vm, frame->slots);
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
			case OP_CLASS: {
				push(vm, OBJ_VAL(newClass(vm, READ_STRING())));
				break;
			}
			case OP_INHERIT: {
				Value superclass = peek(vm, 1);
				if (!IS_CLASS(superclass)) {
					runtimeError(vm, "Superclass must be a class.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjClass* subclass = AS_CLASS(peek(vm, 0));
				// Copy-down inheritance. Possible because a class is closed once declared (can't add more methods afterwards).
				tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
				pop(vm); // subclass
				break;
			}
			case OP_METHOD:
				defineMethod(vm, READ_STRING());
				break;
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
	g_vm = vm;

	resetStack(vm);
	vm->objects = NULL;
	vm->bytesAllocated = 0;
	vm->nextGC = 1024 * 1024; // #todo-gc: Hard-coded initial value

	vm->grayCount = 0;
	vm->grayCapacity = 0;
	vm->grayStack = NULL;

	initTable(&vm->globals);
	initTable(&vm->strings);

	vm->initString = NULL; // This is necessary as copyString() might trigger GC.
	vm->initString = copyString(vm, "init", 4);

	defineNative(vm, "clock", clockNative);
	defineNative(vm, "readFile", readFileNative);
}

void freeVM(VM* vm) {
	freeTable(&vm->globals);
	freeTable(&vm->strings);
	vm->initString = NULL;
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
