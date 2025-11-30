#include "common.h"
#include "chunk.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

typedef struct {
    Token_t previous;
    Token_t current;
    bool hadError;
    bool panicMode;
} Parser_t;

typedef enum {
    PREC_NONE,
    PREC_COMMA,         // ,
    PREC_TERNARY,       // ? :
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // ==  !=
    PREC_BITWISE,       // | & ^
    PREC_COMPARISON,    // < > <= >=
    PREC_BITSHIFT,      // << >>
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // - !
    PREC_EXP,           // **
    PREC_PREFIX,        // ++ --
    PREC_INDEX,         // []
    PREC_POSTFIX,       // ++ --
    PREC_CALL,          // . ()
    PREC_PRIMARY
} Precedence_t;

typedef void (* ParseFn_t)(void);

typedef struct {
    ParseFn_t prefix;
    ParseFn_t infix;
    Precedence_t precedence;
} ParseRule_t;

Parser_t parser;
Chunk_t *compilingChunk;

static void errorAt(Token_t *token, const char *msg) {
    if (parser.panicMode) return;   // suppress follow-on errors while in panic mode
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end.");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at %*.s", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", msg);
    parser.hadError = true;
}

static void error(const char *msg) {
    errorAt(&parser.previous, msg);
}

static void errorAtCurrent(const char *msg) {
    errorAt(&parser.current, msg);
}

static bool check(TokenType_e type) {
    return parser.current.type == type;
}

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static bool match(TokenType_e type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static void consume(TokenType_e type, const char *msg) {
    if (parser.current.type != type) {
        errorAtCurrent(msg);
        return;
    }
    advance();
}

static Chunk_t *currentChunk(void) {
    return compilingChunk;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitVarLenInstr(unsigned idx, unsigned shortThreshold, uint8_t instr0, uint8_t instr1) {
    if (idx < shortThreshold) {
        emitBytes(instr0, (uint8_t)idx);
    } else {
        emitByte(instr1);
        emitByte((idx >> 16) & MASK);
        emitByte((idx >> 8) & MASK);
        emitByte(idx & MASK);
    }
}

static void emitReturn(void) {
    emitByte(OP_RETURN);
}

static void emitLongConstant(int idx) {
    //    3 least significant bytes of idx must be written to bytecode chunk
    //    e.g. index = 257
    //         index =    0000 0000 0000 0001 0000 0001
    //         index =       0    0    0    1    0    1

    emitByte(OP_CONSTANT_LONG);
    emitByte(((unsigned)idx >> 16) & MASK);
    emitByte(((unsigned)idx >> 8) & MASK);
    emitByte((unsigned)idx & MASK);
}

static void endCompiler(void) {
    emitReturn();
}

static void emitConstant(Value_t value) {
    if (currentChunk()->constants.capacity > CONSTANT_POOL_LONG_LEN_MAX) {
        error("Too many constants in one chunk.");
        return;
    }
    int constantIdx = addConstant(currentChunk(), value);
    if (constantIdx > UINT8_MAX) {
        emitLongConstant(constantIdx);
        return;
    }
    emitBytes(OP_CONSTANT, (uint8_t)constantIdx);
}

static void expression(void);
static void parsePrecedence(Precedence_t precedence);
static ParseRule_t *getRule(TokenType_e type);

static void expression(void) {
    parsePrecedence(PREC_COMMA);
}

static void number(void) {
    double value = strtod(parser.previous.start, NULL);
    double tolerance = 1e-10;
    if (fabs(value) <= tolerance) {
        emitByte(OP_ZERO);
    } else if (fabs(value - 1) <= tolerance) {
        emitByte(OP_ONE);
    } else if (value < 0 && fabs(value + 1) <= tolerance) {
        emitByte(OP_NEG_ONE);
    } else {
        emitConstant(NUMBER_VAL(value));
    }
}

static void string(void) {
    emitConstant(OBJ_VAL(makeString(parser.previous.start + 1, 
                                     parser.previous.length - 2)));
}

static unsigned identifierConstant(Token_t *identifier);
static void namedVariable(Token_t name) {
    unsigned idx = identifierConstant(&name);   // <- all we care about is providing the correct string key to tableGet(); doesn't matter if it's a copy as long as chars are same
    emitVarLenInstr(idx, CONSTANT_POOL_SHORT_LEN_MAX, OP_ACCESS_GLOBAL, OP_ACCESS_GLOBAL_LONG);
}

static void variable(void) {
    namedVariable(parser.previous);
}

static void grouping(void) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(void) {
    TokenType_e operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        default: return;    // unreachable
    }
}

static void binary(void) {
    TokenType_e operatorType = parser.previous.type;
    ParseRule_t *rule = getRule(operatorType);
    parsePrecedence((Precedence_t)rule->precedence + 1);

    switch (operatorType) {
        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        case TOKEN_GREATER:         emitByte(OP_GT); break;
        case TOKEN_LESS:            emitByte(OP_LT); break;
        case TOKEN_EQUAL_EQUAL:     emitByte(OP_EQ); break;
        /**
         * a >= b same as !(a < b)
         * a <= b same as !(a > b)
         * a != b same as !(a == b)
         */
        case TOKEN_GREATER_EQUAL:   emitBytes(OP_LT, OP_NOT); break;
        case TOKEN_LESS_EQUAL:      emitBytes(OP_GT, OP_NOT); break;
        case TOKEN_BANG_EQUAL:      emitBytes(OP_EQ, OP_NOT); break;
        default: return;    // unreachable
    }
}

static void ternary1(void) {
    // we just scanned a question mark...
    emitByte(OP_QMARK);
    expression();   // true branch
}

static void ternary2(void) {
    // we just scanned a colon...
    emitByte(OP_COLON);
    expression();   // false branch;
    emitByte(OP_ENDTERNARY);
}

static void literal(void) {
    switch (parser.previous.type) {
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        default: return;    // unreachable
    }
}

// side note: this is called designated initializer syntax (C99)
ParseRule_t rules[] = {
    [TOKEN_LEFT_PAREN]      =  {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN]     =  {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE]      =  {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE]     =  {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACK]      =  {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACK]     =  {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA]           =  {NULL, NULL, PREC_COMMA},
    [TOKEN_EQUAL]           =  {NULL, NULL, PREC_ASSIGNMENT},
    [TOKEN_QUESTION_MARK]   =  {NULL, NULL, PREC_TERNARY},
    [TOKEN_DOT]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS]           =  {unary, binary, PREC_TERM},
    [TOKEN_PLUS]            =  {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON]       =  {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH]           =  {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR]            =  {NULL, binary, PREC_FACTOR},
    [TOKEN_MODULO]          =  {NULL, binary, PREC_FACTOR},
    [TOKEN_QUESTION_MARK]   =  {NULL, ternary1, PREC_TERNARY},
    [TOKEN_COLON]           =  {NULL, ternary2, PREC_TERNARY},
    [TOKEN_BITWISE_AND]     =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BITWISE_OR]      =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BITWISE_XOR]     =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BANG]            =  {unary, NULL, PREC_UNARY},
    [TOKEN_BANG_EQUAL]      =  {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL]           =  {NULL, NULL, PREC_ASSIGNMENT},
    [TOKEN_EQUAL_EQUAL]     =  {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER]         =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS]            =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_BITSHIFT_LEFT]   =  {NULL, binary, PREC_BITSHIFT},
    [TOKEN_BITSHIFT_RIGHT]  =  {NULL, binary, PREC_BITSHIFT},
    [TOKEN_STAR_STAR]       =  {NULL, binary, PREC_EXP},
    [TOKEN_IDENTIFIER]      =  {variable, NULL, PREC_NONE},
    [TOKEN_STRING]          =  {string, NULL, PREC_NONE},
    [TOKEN_INTERPOLATION]   =  {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER]          =  {number, NULL, PREC_NONE},
    [TOKEN_AND]             =  {NULL, binary, PREC_AND},
    [TOKEN_OR]              =  {NULL, binary, PREC_OR},
    [TOKEN_BREAK]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE]            =  {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE]           =  {literal, NULL, PREC_NONE},
    [TOKEN_FUN]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_FOR]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_FOREACH]         =  {NULL, NULL, PREC_NONE},
    [TOKEN_NIL]             =  {literal, NULL, PREC_NONE},
    [TOKEN_PRINT]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN]          =  {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_THIS]            =  {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE]            =  {literal, NULL, PREC_NONE},
    [TOKEN_VAR]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE]        =  {NULL, NULL, PREC_NONE},
    [TOKEN_EOF]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_NOT_FOUND]       =  {NULL, NULL, PREC_NONE},
};

static void expression(void);
static void declaration(void);
static void parsePrecedence(Precedence_t precedence) {
    advance();
    ParseFn_t prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expression requires prefix rule.");
        return;
    }

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn_t infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static ParseRule_t *getRule(TokenType_e type) {
    return &rules[type];
}

static void synchronize(void) {
    parser.panicMode = false;
    while (!match(TOKEN_EOF)) {
        if (match(TOKEN_SEMICOLON)) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ; // do nothing
        }
        advance();
    }
}

static void printStatement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected a ';'.");
    emitByte(OP_PRINT);
}

static void expressionStatement(void) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected a ';'.");
    emitByte(OP_POP);
}

static unsigned makeConstant(Value_t value) {
    int idx = addConstant(currentChunk(), value);
    if (idx >= CONSTANT_POOL_LONG_LEN_MAX) {
        fprintf(stderr, "Constant pool is too large.");
        exit(EXIT_FAILURE);
    }

    return (unsigned)idx;
}

static unsigned identifierConstant(Token_t *identifier) {
    return makeConstant(
        OBJ_VAL(makeString(identifier->start, identifier->length))
    );
}

static unsigned parseVariableName(const char *errMsg) {
    consume(TOKEN_IDENTIFIER, "Expect an indentifier");
    return identifierConstant(&parser.previous);
}

static void defineVariable(unsigned global) {
    emitVarLenInstr(global, CONSTANT_POOL_SHORT_LEN_MAX, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}

static void varDeclaration(void) {
    unsigned global = parseVariableName("Expect a variable name");

    if (match(TOKEN_EQUAL)) {
        expression();   // <- a value will be pushed to stack
    } else {
        emitByte(OP_NIL);   // <- NULL will be pushed to stack
    }

    consume(TOKEN_SEMICOLON, "Expect a ';'.");
    defineVariable(global);
}

static void statement(void) {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else {
        expressionStatement();
    }
}

static void declaration(void) {
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

bool compile(const char *source, Chunk_t *chunk) {
    initScanner(source);

    parser.hadError = false;
    parser.panicMode = false;
    compilingChunk = chunk;

    #ifdef DEBUG_SCANNER
    int line = 1;
    for (;;) {
        Token_t token = scanToken();
        if (token.line != line) {
            printf("%04d ", token.line);
            line = token.line;
        } else {
            printf(" | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
    #endif

    advance();
    while (!match(TOKEN_EOF)) {
        /**
         * Compile Lox script.
         * - Convert sequence of tokens to sequence of declarations
         * - Sequence of declarations encoded in bytecode
         */
        declaration();
    }
    endCompiler();

    #ifdef DEBUG_CHUNK
    #include "debug.h"
    disassembleChunk(currentChunk(), "code");
    #endif

    return !parser.hadError;
}