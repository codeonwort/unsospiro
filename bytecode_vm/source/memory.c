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
		markValue(*slot);
	}

	for (int i = 0; i < vm->frameCount; ++i) {
		markObject((Obj*)vm->frames[i].closure);
	}

	for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		markObject((Obj*)upvalue);
	}

	markTable(&(vm->globals));
	markCompilerRoots();
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

void markObject(Obj* object) {
	if (object == NULL) return;
#if DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif

	object->isMarked = true;
}

void markValue(Value value) {
	if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

void collectGarbage(VM* vm) {
#if DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif

	markRoots(vm);

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
}
