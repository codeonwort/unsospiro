#include "compiler.h"
#include "common.h"
#include "scanner.h"
#if DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,         // or
	PREC_AND,        // and
	PREC_EQUALITY,   // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
	PREC_UNARY,      // ! -
	PREC_CALL,       // . ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser* parser);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Chunk* compilingChunk; // #todo: No global variable

static Chunk* currentChunk() {
	return compilingChunk;
}

static void errorAt(Parser* parser, Token* token, const char* message) {
	if (parser->panicMode) return;
	parser->panicMode = true;
	fprintf_s(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf_s(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		//
	} else {
		fprintf_s(stderr, " at '%.*s", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser->hadError = true;
}

static void errorAtCurrent(Parser* parser, const char* message) {
	errorAt(parser, &(parser->current), message);
}

static void error(Parser* parser, const char* message) {
	errorAt(parser, &(parser->previous), message);
}

static void advance(Parser* parser) {
	parser->previous = parser->current;

	for (;;) {
		parser->current = scanToken();
		if (parser->current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser, parser->current.start);
	}
}

static void consume(Parser* parser, TokenType type, const char* message) {
	if (parser->current.type == type) {
		advance(parser);
		return;
	}
	errorAtCurrent(parser, message);
}

static void emitByte(Parser* parser, uint8_t byte) {
	writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
	emitByte(parser, byte1);
	emitByte(parser, byte2);
}

static void emitReturn(Parser* parser) {
	emitByte(parser, OP_RETURN);
}

static uint8_t makeConstant(Parser* parser, Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error(parser, "Too many constants in one chunk.");
		return 0;
	}
	return (uint8_t)constant;
}

static void emitConstant(Parser* parser, Value value) {
	emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void endCompiler(Parser* parser) {
	emitReturn(parser);
#if DEBUG_PRINT_CODE
	if (!parser->hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static void expression(Parser* parser);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Parser* parser, Precedence precedence);

static void binary(Parser* parser) {
	TokenType operatorType = parser->previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence(parser, (Precedence)(rule->precedence + 1));

	switch (operatorType) {
		case TOKEN_BANG_EQUAL: emitBytes(parser, OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL: emitByte(parser, OP_EQUAL); break;
		case TOKEN_GREATER: emitByte(parser, OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitBytes(parser, OP_LESS, OP_NOT); break;
		case TOKEN_LESS: emitByte(parser, OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitBytes(parser, OP_GREATER, OP_NOT); break;
		case TOKEN_PLUS:  emitByte(parser, OP_ADD); break;
		case TOKEN_MINUS: emitByte(parser, OP_SUBTRACT); break;
		case TOKEN_STAR:  emitByte(parser, OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(parser, OP_DIVIDE); break;
		default: return;
	}
}

static void literal(Parser* parser) {
	switch (parser->previous.type) {
		case TOKEN_FALSE: emitByte(parser, OP_FALSE); break;
		case TOKEN_NIL: emitByte(parser, OP_NIL); break;
		case TOKEN_TRUE: emitByte(parser, OP_TRUE); break;
		default: return;
	}
}

static void grouping(Parser* parser) {
	expression(parser);
	consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* parser) {
	double value = strtod(parser->previous.start, NULL);
	emitConstant(parser, NUMBER_VAL(value));
}

static void string(Parser* parser) {
	emitConstant(parser, OBJ_VAL(copyString(parser->previous.start + 1, parser->previous.length - 2)));
}

static void unary(Parser* parser) {
	TokenType operatorType = parser->previous.type;

	// Compile operands
	parsePrecedence(parser, PREC_UNARY);
	// Emit operator instruction
	switch (operatorType) {
		case TOKEN_BANG: emitByte(parser, OP_NOT); break;
		case TOKEN_MINUS: emitByte(parser, OP_NEGATE); break;
		default: return;
	}
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
	[TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
	[TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
	[TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
	[TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
	[TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
	[TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
	[TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
	[TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
	[TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
	[TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parsePrecedence(Parser* parser, Precedence precedence) {
	advance(parser);
	ParseFn prefixRule = getRule(parser->previous.type)->prefix;
	if (prefixRule == NULL) {
		error(parser, "Expect expression.");
		return;
	}
	prefixRule(parser);

	while (precedence <= getRule(parser->current.type)->precedence) {
		advance(parser);
		ParseFn infixRule = getRule(parser->previous.type)->infix;
		infixRule(parser);
	}
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void expression(Parser* parser) {
	parsePrecedence(parser, PREC_ASSIGNMENT);
}

bool compile(const char* source, Chunk* chunk) {
	Parser parser;
	compilingChunk = chunk;

	initScanner(source);

	parser.hadError = false;
	parser.panicMode = false;

	advance(&parser);
	expression(&parser);
	consume(&parser, TOKEN_EOF, "Expect end of expression.");
	endCompiler(&parser);

	return !parser.hadError;
}
