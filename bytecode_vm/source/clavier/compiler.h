#pragma once

#include "object.h"
#include "vm.h"

ObjFunction* compile(VM* vm, const char* source);
void markCompilerRoots();
