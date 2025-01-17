#include "compiler.h"
#include "memory.h"
#include "vm.h"

#if DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif


#include <stdlib.h>

static void freeObject(Obj* object) {
	switch (object->type) {
		case OBJ_CLOSURE: {
			// A closure does not own its function, so does not free it.
			// To free a function, all references to it should be gone first.
			// That's hard to track, so GC will handle it.
			ObjClosure* closure = (ObjClosure*)object;
			FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
			FREE(ObjClosure, object);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			freeChunk(&function->chunk);
			FREE(ObjFunction, object);
			break;
		}
		case OBJ_NATIVE: {
			FREE(ObjNative, object);
			break;
		}
		case OBJ_STRING: {
			ObjString* string = (ObjString*)object;
			FREE_ARRAY(char, string->chars, string->length + 1);
			FREE(ObjString, object);
			break;
		}
		case OBJ_UPVALUE: {
			// Multiple closure might close over the same variable,
			// so ObjUpvalue does not own the variable it refers to.
			FREE(ObjUpvalue, object);
			break;
		}
	}
}

static void markRoots(VM* vm) {
	for (Value* slot = vm->stack; slot < vm->stackTop; ++slot) {
		markValue(vm, *slot);
	}

	for (int i = 0; i < vm->frameCount; ++i) {
		markObject(vm, (Obj*)vm->frames[i].closure);
	}

	for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		markObject(vm, (Obj*)upvalue);
	}

	markTable(vm, &(vm->globals));
	markCompilerRoots();
}

static void markArray(VM* vm, ValueArray* array) {
	for (int i = 0; i < array->count; ++i) {
		markValue(vm, array->values[i]);
	}
}

static void blackenObject(VM* vm, Obj* object) {
#if DEBUG_LOG_GC
	printf("%p blacken ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif

	switch (object->type) {
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			markObject(vm, (Obj*)closure->function);
			for (int i = 0; i < closure->upvalueCount; ++i) {
				markObject(vm, (Obj*)closure->upvalues[i]);
			}
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			markObject(vm, (Obj*)(function->name));
			markArray(vm, &(function->chunk.constants));
			break;
		}
		case OBJ_UPVALUE:
			markValue(vm, ((ObjUpvalue*)object)->closed);
			break;
		// Native functions and strings have no outgoing references;
		case OBJ_NATIVE:
		case OBJ_STRING:
			break;
	}
}

static void traceReferences(VM* vm) {
	while (vm->grayCount > 0) {
		Obj* object = vm->grayStack[--vm->grayCount];
		blackenObject(vm, object);
	}
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
	if (newSize > oldSize) {
#if DEBUG_STRESS_GC
		collectGarbage(g_vm);
#endif
	}

	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	if (result == NULL) exit(1); // Out of Memory
	return result;
}

void markObject(VM* vm, Obj* object) {
	if (object == NULL) return;
	if (object->isMarked) return; // Cyclic reference
#if DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif

	object->isMarked = true;

	if (vm->grayCapacity < vm->grayCount + 1) {
		vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
		// Memory for gray stack is not managed by GC.
		// #todo: If realloc() fails original memory is not freed. We do exit() so it doesn't matter for now.
		vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
	}

	// #todo: Failed to allocate gray stack.
	if (vm->grayStack == NULL) exit(1);

	vm->grayStack[vm->grayCount++] = object;
}

void markValue(VM* vm, Value value) {
	if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

void collectGarbage(VM* vm) {
#if DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif

	markRoots(vm);
	traceReferences(vm);

#if DEBUG_LOG_GC
	printf("-- gc end\n");
#endif
}

void freeObjects(VM* vm) {
	Obj* object = vm->objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
	free(vm->grayStack);
}
