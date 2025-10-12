#include "common.h"
#include "chunk.h"
#include "compiler.h"
#include "scanner.h"

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

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
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

static void emitReturn(void) {
    emitByte(OP_RETURN);
}

static void emitLongConstant(int idx) {
    //    3 least significant bytes of idx must be written to bytecode chunk
    //    e.g. index = 257
    //         index =    0000 0000 0000 0001 0000 0001
    //         index =       0    0    0    1    0    1

    emitByte(OP_CONSTANT_LONG);
    emitByte(((unsigned)idx >> 16) & 0x000000FF);
    emitByte(((unsigned)idx >> 8) & 0x000000FF);
    emitByte((unsigned)idx & 0x000000FF);
}

static void endCompiler(void) {
    emitReturn();
}

static void emitConstant(double value) {
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
    emitConstant(value);
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
        default: return;    // unreachable
    }
}

static void binary(void) {
    TokenType_e operatorType = parser.previous.type;
    ParseRule_t *rule = getRule(operatorType);
    parsePrecedence((Precedence_t)rule->precedence + 1);

    switch (operatorType) {
        case TOKEN_PLUS:    emitByte(OP_ADD); break;
        case TOKEN_MINUS:   emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:    emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:   emitByte(OP_ADD); break;
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
    [TOKEN_QUESTION_MARK]   =  {NULL, NULL, PREC_NONE},
    [TOKEN_COLON]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_BITWISE_AND]     =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BITWISE_OR]      =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BITWISE_XOR]     =  {NULL, binary, PREC_BITWISE},
    [TOKEN_BANG]            =  {unary, NULL, PREC_UNARY},
    [TOKEN_BANG_EQUAL]      =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_EQUAL]           =  {NULL, NULL, PREC_ASSIGNMENT},
    [TOKEN_EQUAL_EQUAL]     =  {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER]         =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS]            =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      =  {NULL, binary, PREC_COMPARISON},
    [TOKEN_BITSHIFT_LEFT]   =  {NULL, binary, PREC_BITSHIFT},
    [TOKEN_BITSHIFT_RIGHT]  =  {NULL, binary, PREC_BITSHIFT},
    [TOKEN_STAR_STAR]       =  {NULL, binary, PREC_EXP},
    [TOKEN_IDENTIFIER]      =  {NULL, NULL, PREC_NONE},
    [TOKEN_STRING]          =  {NULL, NULL, PREC_NONE},
    [TOKEN_INTERPOLATION]   =  {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER]          =  {number, NULL, PREC_NONE},
    [TOKEN_AND]             =  {NULL, binary, PREC_AND},
    [TOKEN_OR]              =  {NULL, binary, PREC_OR},
    [TOKEN_BREAK]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE]            =  {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_FUN]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_FOR]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_FOREACH]         =  {NULL, NULL, PREC_NONE},
    [TOKEN_NIL]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN]          =  {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_THIS]            =  {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE]            =  {NULL, NULL, PREC_NONE},
    [TOKEN_VAR]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE]        =  {NULL, NULL, PREC_NONE},
    [TOKEN_EOF]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR]           =  {NULL, NULL, PREC_NONE},
    [TOKEN_NOT_FOUND]       =  {NULL, NULL, PREC_NONE},
};

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
    expression();
    consume(TOKEN_EOF, "Expect end or expression.");
    endCompiler();
    return !parser.hadError;
}