#include "compiler.h"
#include "common.h"
#include "scanner.h"

#include <stdio.h>

void compile(const char* source) {
	initScanner(source);
	int line = -1;
	for (;;) {
		Token token = scanToken();
		if (token.line != line) {
			printf("%4d", token.line);
			line = token.line;
		} else {
			printf("  | ");
		}
		// %.*s maps to first (token.length) characters at (token.start)
		printf("%2d '%.*s'\n", token.type, token.length, token.start);

		if (token.type == TOKEN_EOF) break;
	}
}
