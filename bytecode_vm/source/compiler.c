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

typedef struct {
	Token name;
	int depth; // 0 = global scope, 1 = top level block, ...
} Local;

typedef struct {
	Local locals[UINT8_COUNT]; // #todo: Support more than 256 local variables
	int localCount;
	int scopeDepth;
} Compiler;

// Used in compile() to pass parameters.
typedef struct {
	Scanner* scanner;
	Compiler* compiler;
	VM* vm;
	Parser* parser;
	Chunk* currentChunk;
} Context;

typedef void (*ParseFn)(Context* ctx, bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

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

static void advance(Context* ctx) {
	Parser* parser = ctx->parser;

	parser->previous = parser->current;

	for (;;) {
		parser->current = scanToken(ctx->scanner);
		if (parser->current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser, parser->current.start);
	}
}

static void consume(Context* ctx, TokenType type, const char* message) {
	if (ctx->parser->current.type == type) {
		advance(ctx);
		return;
	}
	errorAtCurrent(ctx->parser, message);
}

static bool check(Parser* parser, TokenType type) {
	return parser->current.type == type;
}

static bool match(Context* ctx, TokenType type) {
	if (!check(ctx->parser, type)) return false;
	advance(ctx);
	return true;
}

static void emitByte(Context* ctx, uint8_t byte) {
	Parser* parser = ctx->parser;
	Chunk* currentChunk = ctx->currentChunk;

	writeChunk(currentChunk, byte, parser->previous.line);
}

static void emitBytes(Context* ctx, uint8_t byte1, uint8_t byte2) {
	emitByte(ctx, byte1);
	emitByte(ctx, byte2);
}

static void emitLoop(Context* ctx, int loopStart) {
	emitByte(ctx, OP_LOOP);

	int offset = ctx->currentChunk->count - loopStart + 2;
	if (offset > UINT16_MAX) error(ctx->parser, "Loop body too large.");

	emitByte(ctx, (offset >> 8) & 0xff);
	emitByte(ctx, offset & 0xff);
}

static int emitJump(Context* ctx, uint8_t instruction) {
	// Backpatching; here, emit jump instruction first with placeholder offsets.
	// they will be replaced by real offsets after 'then' branch is compiled.
	emitByte(ctx, instruction);
	emitByte(ctx, 0xff);
	emitByte(ctx, 0xff);
	return ctx->currentChunk->count - 2;
}

static void emitReturn(Context* ctx) {
	emitByte(ctx, OP_RETURN);
}

static uint8_t makeConstant(Context* ctx, Value value) {
	int constant = addConstant(ctx->currentChunk, value);
	if (constant > UINT8_MAX) {
		error(ctx->parser, "Too many constants in one chunk.");
		return 0;
	}
	return (uint8_t)constant;
}

static void emitConstant(Context* ctx, Value value) {
	emitBytes(ctx, OP_CONSTANT, makeConstant(ctx, value));
}

static void patchJump(Context* ctx, int offset) {
	Parser* parser = ctx->parser;
	Chunk* currentChunk = ctx->currentChunk;

	// Backpatching; Replace placeholder offsets with real ones.
	int jump = currentChunk->count - offset - 2;

	if (jump > UINT16_MAX) {
		error(parser, "Too much code to jump over.");
	}

	currentChunk->code[offset] = (jump >> 8) & 0xff;
	currentChunk->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
}

static void endCompiler(Context* ctx) {
	emitReturn(ctx);
#if DEBUG_PRINT_CODE
	if (!ctx->parser->hadError) {
		disassembleChunk(ctx->currentChunk, "code");
	}
#endif
}

static void beginScope(Compiler* compiler) {
	compiler->scopeDepth++;
}

static void endScope(Context* ctx) {
	Parser* parser = ctx->parser;
	Compiler* current = ctx->compiler;

	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(ctx, OP_POP);
		current->localCount--;
	}
}

static void expression(Context* ctx);
static void statement(Context* ctx);
static void declaration(Context* ctx);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Context* ctx, Precedence precedence);

static uint8_t identifierConstant(Context* ctx, Token* name) {
	return makeConstant(ctx, OBJ_VAL(copyString(ctx->vm, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return 0 == memcmp(a->start, b->start, a->length);
}

static int resolveLocal(Parser* parser, Compiler* compiler, Token* name) {
	// #todo: Compiler needs to linear search a variable every time. Better way?
	// Traverse in reverse for shadowing.
	for (int i = compiler->localCount - 1; i >= 0; --i) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				// The variable is declared but not defined yet. (see addLocal and defineVariable functions)
				error(parser, "Can't read local variable in its own initializer.");
			}
			return i;
		}
	}
	return -1;
}

static void addLocal(Context* ctx, Token name) {
	if (ctx->compiler->localCount == UINT8_COUNT) {
		error(ctx->parser, "Too many local variables in function.");
		return;
	}

	Local* local = &ctx->compiler->locals[ctx->compiler->localCount++];
	local->name = name;
	local->depth = -1; // Variable is declared but not defined yet. Will be initialized in defineVariable().
}

static void declareVariable(Context* ctx) {
	Compiler* compiler = ctx->compiler;
	if (compiler->scopeDepth == 0) return;

	Token* name = &ctx->parser->previous;
	for (int i = (int)(compiler->localCount) - 1; i >= 0; --i) {
		Local* local = &compiler->locals[i];
		if (local->depth != -1 && local->depth < compiler->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error(ctx->parser, "A variable with this name already exists in this scope.");
		}
	}

	addLocal(ctx, *name);
}

static uint8_t parseVariable(Context* ctx, const char* errorMessage) {
	consume(ctx, TOKEN_IDENTIFIER, errorMessage);

	declareVariable(ctx);
	if (ctx->compiler->scopeDepth > 0) return 0;

	return identifierConstant(ctx, &ctx->parser->previous);
}

static void markInitialized(Compiler* compiler) {
	compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Context* ctx, uint8_t global) {
	if (ctx->compiler->scopeDepth > 0) {
		markInitialized(ctx->compiler);
		return;
	}

	emitBytes(ctx, OP_DEFINE_GLOBAL, global);
}

static void and_(Context* ctx, bool canAssign) {
	int endJump = emitJump(ctx, OP_JUMP_IF_FALSE);

	emitByte(ctx, OP_POP);
	parsePrecedence(ctx, PREC_AND);

	patchJump(ctx, endJump);
}

static void binary(Context* ctx, bool canAssign) {
	TokenType operatorType = ctx->parser->previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence(ctx, (Precedence)(rule->precedence + 1));

	switch (operatorType) {
		case TOKEN_BANG_EQUAL: emitBytes(ctx, OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL: emitByte(ctx, OP_EQUAL); break;
		case TOKEN_GREATER: emitByte(ctx, OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitBytes(ctx, OP_LESS, OP_NOT); break;
		case TOKEN_LESS: emitByte(ctx, OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitBytes(ctx, OP_GREATER, OP_NOT); break;
		case TOKEN_PLUS:  emitByte(ctx, OP_ADD); break;
		case TOKEN_MINUS: emitByte(ctx, OP_SUBTRACT); break;
		case TOKEN_STAR:  emitByte(ctx, OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(ctx, OP_DIVIDE); break;
		default: return;
	}
}

static void literal(Context* ctx, bool canAssign) {
	switch (ctx->parser->previous.type) {
		case TOKEN_FALSE: emitByte(ctx, OP_FALSE); break;
		case TOKEN_NIL: emitByte(ctx, OP_NIL); break;
		case TOKEN_TRUE: emitByte(ctx, OP_TRUE); break;
		default: return;
	}
}

static void grouping(Context* ctx, bool canAssign) {
	expression(ctx);
	consume(ctx, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Context* ctx, bool canAssign) {
	double value = strtod(ctx->parser->previous.start, NULL);
	emitConstant(ctx, NUMBER_VAL(value));
}

static void or_(Context* ctx, bool canAssign) {
	int elseJump = emitJump(ctx, OP_JUMP_IF_FALSE);
	int endJump = emitJump(ctx, OP_JUMP);

	patchJump(ctx, elseJump);
	emitByte(ctx, OP_POP);

	parsePrecedence(ctx, PREC_OR);
	patchJump(ctx, endJump);
}

static void string(Context* ctx, bool canAssign) {
	VM* vm = ctx->vm;
	Parser* parser = ctx->parser;
	emitConstant(ctx, OBJ_VAL(copyString(vm, parser->previous.start + 1, parser->previous.length - 2)));
}

// #todo: Support const var?
static void namedVariable(Context* ctx, Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(ctx->parser, ctx->compiler, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else {
		arg = identifierConstant(ctx, &name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(ctx, TOKEN_EQUAL)) {
		expression(ctx);
		emitBytes(ctx, setOp, (uint8_t)arg);
	} else {
		emitBytes(ctx, getOp, (uint8_t)arg);
	}
}

static void variable(Context* ctx, bool canAssign) {
	namedVariable(ctx, ctx->parser->previous, canAssign);
}

static void unary(Context* ctx, bool canAssign) {
	TokenType operatorType = ctx->parser->previous.type;

	// Compile operands
	parsePrecedence(ctx, PREC_UNARY);
	// Emit operator instruction
	switch (operatorType) {
		case TOKEN_BANG: emitByte(ctx, OP_NOT); break;
		case TOKEN_MINUS: emitByte(ctx, OP_NEGATE); break;
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
	[TOKEN_AND]           = {NULL,     and_,   PREC_AND},
	[TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
	[TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
	[TOKEN_OR]            = {NULL,     or_,    PREC_OR},
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

static void parsePrecedence(Context* ctx, Precedence precedence) {
	Parser* parser = ctx->parser;

	advance(ctx);
	ParseFn prefixRule = getRule(parser->previous.type)->prefix;
	if (prefixRule == NULL) {
		error(parser, "Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(ctx, canAssign);

	while (precedence <= getRule(parser->current.type)->precedence) {
		advance(ctx);
		ParseFn infixRule = getRule(parser->previous.type)->infix;
		infixRule(ctx, canAssign);
	}

	if (canAssign && match(ctx, TOKEN_EQUAL)) {
		error(parser, "Invalid assignment target.");
	}
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void expression(Context* ctx) {
	parsePrecedence(ctx, PREC_ASSIGNMENT);
}

static void block(Context* ctx) {
	Parser* parser = ctx->parser;

	while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
		declaration(ctx);
	}
	consume(ctx, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(Context* ctx) {
	Parser* parser = ctx->parser;

	uint8_t global = parseVariable(ctx, "Expect variable name.");

	if (match(ctx, TOKEN_EQUAL)) {
		expression(ctx);
	} else {
		emitByte(ctx, OP_NIL);
	}
	consume(ctx, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(ctx, global);
}

static void expressionStatement(Context* ctx) {
	expression(ctx);
	consume(ctx, TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(ctx, OP_POP);
}

static void ifStatement(Context* ctx) {
	consume(ctx, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression(ctx);
	consume(ctx, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = emitJump(ctx, OP_JUMP_IF_FALSE);
	emitByte(ctx, OP_POP);
	statement(ctx);

	int elseJump = emitJump(ctx, OP_JUMP);

	patchJump(ctx, thenJump);
	emitByte(ctx, OP_POP);

	if (match(ctx, TOKEN_ELSE)) statement(ctx);
	patchJump(ctx, elseJump);
}

static void printStatement(Context* ctx) {
	expression(ctx);
	consume(ctx, TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(ctx, OP_PRINT);
}

static void whileStatement(Context* ctx) {
	int loopStart = ctx->currentChunk->count;
	consume(ctx, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression(ctx);
	consume(ctx, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int exitJump = emitJump(ctx, OP_JUMP_IF_FALSE);
	emitByte(ctx, OP_POP);
	statement(ctx);
	emitLoop(ctx, loopStart);

	patchJump(ctx, exitJump);
	emitByte(ctx, OP_POP);
}

static void synchronize(Context* ctx) {
	ctx->parser->panicMode = false;

	while (ctx->parser->current.type != TOKEN_EOF) {
		switch (ctx->parser->current.type) {
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

		advance(ctx);
	}
}

static void declaration(Context* ctx) {
	if (match(ctx, TOKEN_VAR)) {
		varDeclaration(ctx);
	} else {
		statement(ctx);
	}

	if (ctx->parser->panicMode) synchronize(ctx);
}

static void statement(Context* ctx) {
	if (match(ctx, TOKEN_PRINT)) {
		printStatement(ctx);
	} else if (match(ctx, TOKEN_IF)) {
		ifStatement(ctx);
	} else if (match(ctx, TOKEN_WHILE)) {
		whileStatement(ctx);
	} else if (match(ctx, TOKEN_LEFT_BRACE)) {
		beginScope(ctx->compiler);
		block(ctx);
		endScope(ctx);
	} else {
		expressionStatement(ctx);
	}
}

bool compile(VM* vm, const char* source, Chunk* chunk) {
	Parser parser;

	Compiler compiler;
	initCompiler(&compiler);

	Scanner scanner;
	initScanner(&scanner, source);

	parser.hadError = false;
	parser.panicMode = false;

	Context ctx;
	ctx.scanner = &scanner;
	ctx.compiler = &compiler;
	ctx.vm = vm;
	ctx.parser = &parser;
	ctx.currentChunk = chunk;

	advance(&ctx);

	while (!match(&ctx, TOKEN_EOF)) {
		declaration(&ctx);
	}

	endCompiler(&ctx);

	return !parser.hadError;
}
