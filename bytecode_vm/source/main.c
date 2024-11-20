#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "debug.h"
#include <stdio.h>

VM vm;

int main(int argc, const char* argv[]) {
    initVM(&vm);

    Chunk chunk;
    initChunk(&chunk);

    // Hand-compile a constant instruction, for now.
    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");
    interpret(&vm, &chunk);
    freeVM(&vm);
    freeChunk(&chunk);

    return 0;
}
