#include "object.h"
#include "table.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(vm, type, objectType) \
	(type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;

	object->next = vm->objects;
	vm->objects = object;
	return object;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
	ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	return closure;
}

ObjFunction* newFunction(VM* vm) {
	ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

ObjNative* newNative(VM* vm, NativeFn function) {
	ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}

static ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	tableSet(&vm->strings, string, NIL_VAL); // Intern every string
	return string;
}

// #todo: Explore hashing algorithms
static uint32_t hashString(const char* key, int length) {
	// FNV-1a hash
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; ++i) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjString* takeString(VM* vm, char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}

	return allocateString(vm, chars, length, hash);
}

ObjString* copyString(VM* vm, const char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL) return interned;

	char* heapChars = ALLOCATE(char, length + 1);
	memcpy_s(heapChars, length, chars, length);
	heapChars[length] = '\0';
	return allocateString(vm, heapChars, length, hash);
}

static void printFunction(ObjFunction* function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_CLOSURE:
			printFunction(AS_CLOSURE(value)->function);
			break;
		case OBJ_FUNCTION:
			printFunction(AS_FUNCTION(value));
			break;
		case OBJ_NATIVE:
			printf_s("<native fn>");
			break;
		case OBJ_STRING:
			printf_s("%s", AS_CSTRING(value));
			break;
	}
}
