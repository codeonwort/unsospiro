#pragma once

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);

// Returns the offset of the next instruction.
int disassembleInstruction(Chunk* chunk, int offset);
