#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef struct VM_t VM;

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

// For each enum:
// - Declare IS_XXX() and AS_XXX() macros in object.h
// - Declare newXXX() in object.h
// - Modify printObject() in object.c
// - Modify freeObject() in memory.c
typedef enum {
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_NATIVE,
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;
};

// Function is first class citizen.
typedef struct {
	Obj obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

// Native functions have side effect and represented in different way than ObjFunction.
typedef Value(*NativeFn)(int argCount, Value* args);

typedef struct {
	Obj obj;
	NativeFn function;
} ObjNative;

struct ObjString {
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;
};

typedef struct {
	Obj obj;
	ObjFunction* function;
} ObjClosure;

ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjFunction* newFunction(VM* vm);
ObjNative* newNative(VM* vm, NativeFn function);
ObjString* takeString(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
void printObject(Value value);

// Should not be a macro, if so 'value' will be evaluated twice.
static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
