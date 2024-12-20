#include "compiler.h"
#include "common.h"
#include "scanner.h"
#if DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// statement   -> exprStmt | printStmt ;
// declaration -> varDecl | statement ;
//

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

typedef void (*ParseFn)(VM* vm, Parser* parser, bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth; // 0 = global scope, 1 = top level block, ...
} Local;

typedef struct {
	Local locals[UINT8_COUNT];
	int localCount;
	int scopeDepth;
} Compiler;

// #todo: Don't use global variables. Maybe need a context struct for vm + parser + these variables?
Compiler* current = NULL;
Chunk* compilingChunk;

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

static bool check(Parser* parser, TokenType type) {
	return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type) {
	if (!check(parser, type)) return false;
	advance(parser);
	return true;
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

static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

static void endCompiler(Parser* parser) {
	emitReturn(parser);
#if DEBUG_PRINT_CODE
	if (!parser->hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static void beginScope() {
	current->scopeDepth++;
}

static void endScope(Parser* parser) {
	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(parser, OP_POP);
		current->localCount--;
	}
}

static void expression(VM* vm, Parser* parser);
static void statement(VM* vm, Parser* parser);
static void declaration(VM* vm, Parser* parser);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VM* vm, Parser* parser, Precedence precedence);

static uint8_t identifierConstant(VM* vm, Parser* parser, Token* name) {
	return makeConstant(parser, OBJ_VAL(copyString(vm, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return 0 == memcmp(a->start, b->start, a->length);
}

static int resolveLocal(Compiler* compiler, Token* name) {
	// Traverse in reverse for shadowing.
	for (int i = compiler->localCount - 1; i >= 0; --i) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			return i;
		}
	}
	return -1;
}

static void addLocal(Parser* parser, Token name) {
	if (current->localCount == UINT8_COUNT) {
		error(parser, "Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = current->scopeDepth;
}

static void declareVariable(VM* vm, Parser* parser) {
	if (current->scopeDepth == 0) return;

	Token* name = &parser->previous;
	for (int i = (int)(current->localCount) - 1; i >= 0; --i) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error(parser, "A variable with this name already exists in this scope.");
		}
	}

	addLocal(parser, *name);
}

static uint8_t parseVariable(VM* vm, Parser* parser, const char* errorMessage) {
	consume(parser, TOKEN_IDENTIFIER, errorMessage);

	declareVariable(vm, parser);
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(vm, parser, &parser->previous);
}

static void defineVariable(Parser* parser, uint8_t global) {
	if (current->scopeDepth > 0) {
		return;
	}

	emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

static void binary(VM* vm, Parser* parser, bool canAssign) {
	TokenType operatorType = parser->previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence(vm, parser, (Precedence)(rule->precedence + 1));

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

static void literal(VM* vm, Parser* parser, bool canAssign) {
	switch (parser->previous.type) {
		case TOKEN_FALSE: emitByte(parser, OP_FALSE); break;
		case TOKEN_NIL: emitByte(parser, OP_NIL); break;
		case TOKEN_TRUE: emitByte(parser, OP_TRUE); break;
		default: return;
	}
}

static void grouping(VM* vm, Parser* parser, bool canAssign) {
	expression(vm, parser);
	consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VM* vm, Parser* parser, bool canAssign) {
	double value = strtod(parser->previous.start, NULL);
	emitConstant(parser, NUMBER_VAL(value));
}

static void string(VM* vm, Parser* parser, bool canAssign) {
	emitConstant(parser, OBJ_VAL(copyString(vm, parser->previous.start + 1, parser->previous.length - 2)));
}

static void namedVariable(VM* vm, Parser* parser, Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else {
		arg = identifierConstant(vm, parser, &name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(parser, TOKEN_EQUAL)) {
		expression(vm, parser);
		emitBytes(parser, setOp, (uint8_t)arg);
	} else {
		emitBytes(parser, getOp, (uint8_t)arg);
	}
}

static void variable(VM* vm, Parser* parser, bool canAssign) {
	namedVariable(vm, parser, parser->previous, canAssign);
}

static void unary(VM* vm, Parser* parser, bool canAssign) {
	TokenType operatorType = parser->previous.type;

	// Compile operands
	parsePrecedence(vm, parser, PREC_UNARY);
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
	[TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
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

static void parsePrecedence(VM* vm, Parser* parser, Precedence precedence) {
	advance(parser);
	ParseFn prefixRule = getRule(parser->previous.type)->prefix;
	if (prefixRule == NULL) {
		error(parser, "Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(vm, parser, canAssign);

	while (precedence <= getRule(parser->current.type)->precedence) {
		advance(parser);
		ParseFn infixRule = getRule(parser->previous.type)->infix;
		infixRule(vm, parser, canAssign);
	}

	if (canAssign && match(parser, TOKEN_EQUAL)) {
		error(parser, "Invalid assignment target.");
	}
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void expression(VM* vm, Parser* parser) {
	parsePrecedence(vm, parser, PREC_ASSIGNMENT);
}

static void block(VM* vm, Parser* parser) {
	while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
		declaration(vm, parser);
	}
	consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(VM* vm, Parser* parser) {
	uint8_t global = parseVariable(vm, parser, "Expect variable name.");

	if (match(parser, TOKEN_EQUAL)) {
		expression(vm, parser);
	} else {
		emitByte(parser, OP_NIL);
	}
	consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(parser, global);
}

static void expressionStatement(VM* vm, Parser* parser) {
	expression(vm, parser);
	consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(parser, OP_POP);
}

static void printStatement(VM* vm, Parser* parser) {
	expression(vm, parser);
	consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(parser, OP_PRINT);
}

static void synchronize(Parser* parser) {
	parser->panicMode = false;

	while (parser->current.type != TOKEN_EOF) {
		switch (parser->current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUN:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_PRINT:
			case TOKEN_RETURN:
				return;
			default:
				; // Do nothing
		}

		advance(parser);
	}
}

static void declaration(VM* vm, Parser* parser) {
	if (match(parser, TOKEN_VAR)) {
		varDeclaration(vm, parser);
	} else {
		statement(vm, parser);
	}

	if (parser->panicMode) synchronize(parser);
}

static void statement(VM* vm, Parser* parser) {
	if (match(parser, TOKEN_PRINT)) {
		printStatement(vm, parser);
	} else if (match(parser, TOKEN_LEFT_BRACE)) {
		beginScope();
		block(vm, parser);
		endScope(parser);
	} else {
		expressionStatement(vm, parser);
	}
}

bool compile(VM* vm, const char* source, Chunk* chunk) {
	Parser parser;

	Compiler compiler;
	initCompiler(&compiler);

	compilingChunk = chunk;

	initScanner(source);

	parser.hadError = false;
	parser.panicMode = false;

	advance(&parser);

	while (!match(&parser, TOKEN_EOF)) {
		declaration(vm, &parser);
	}

	endCompiler(&parser);

	return !parser.hadError;
}
