#include "scanner.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

// #todo: Should be not a global resource
Scanner scanner;

void initScanner(const char* source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}
