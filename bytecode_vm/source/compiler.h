#pragma once

#include "object.h"
#include "vm.h"

bool compile(VM* vm, const char* source, Chunk* chunk);
