#include "memory.h"
#include "vm.h"

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

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	if (result == NULL) exit(1); // Out of Memory
	return result;
}

void freeObjects(VM* vm) {
	Obj* object = vm->objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
}
