#pragma once

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

typedef struct VM_t VM;

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

// For each enum:
// - Declare IS_XXX() and AS_XXX() macros in object.h
// - Declare newXXX() in object.h
// - Modify printObject() in object.c
// - Modify freeObject() in memory.c
typedef enum {
	OBJ_CLASS,
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_INSTANCE,
	OBJ_NATIVE,
	OBJ_STRING,
	OBJ_UPVALUE,
} ObjType;

// Base type for object representation in VM.
// - Derived types include Obj as the first field.
// Each derived type may contain additional fields. Those fields should be handled in:
// - newXXX() for initialization.
// - freeObject() for deallocation.
// - blackenObject() for GC marking.
struct Obj {
	// #todo: Compact layout
	ObjType type;
	bool isMarked;
	struct Obj* next;
};

// Function is first class citizen.
typedef struct {
	Obj obj;
	int arity;
	int upvalueCount;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

// Native functions have side effect and represented in different way than ObjFunction.
typedef Value(*NativeFn)(VM* vm, int argCount, Value* args);

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

// Runtime representation of an upvalue.
typedef struct ObjUpvalue {
	Obj obj;
	// It's a pointer to a Value, so assigning something to a variable
	// captured by an upvalue actually assigns it to the real variable, not a copy of it.
	Value* location;
	Value closed;
	struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	// ObjFunction also holds upvalueCount, but it's duplicated here for GC.
	int upvalueCount;
} ObjClosure;

typedef struct {
	Obj obj;
	ObjString* name;
	Table methods;
} ObjClass;

typedef struct {
	Obj obj;
	ObjClass* klass;
	Table fields;
} ObjInstance;

ObjClass* newClass(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjFunction* newFunction(VM* vm);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjNative* newNative(VM* vm, NativeFn function);
ObjString* takeString(VM* vm, char* chars, int length);
// length does not include the terminating null.
ObjString* copyString(VM* vm, const char* chars, int length);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
void printObject(Value value);

// Should not be a macro, if so 'value' will be evaluated twice.
static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
