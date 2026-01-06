#include "common.h"
#include "chunk.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"


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

typedef void (* ParseFn_t)(bool);

typedef struct {
    ParseFn_t prefix;
    ParseFn_t infix;
    Precedence_t precedence;
} ParseRule_t;

/**
 * Each local variable has a name and a depth.
 * The depth is simply its scope depth.
 * This allows us to know which vars to discard when a scope ends.
 */
typedef struct {
    Token_t name;
    int depth;
} Local_t;

/**
 * Simple, flat array of all locals that are in scope during each point of the compilation process.
 * Locals are ordered in the array in order their declarations appear in the code.
 */
typedef struct {
    Local_t *locals;
    int localCount; // number of locals in scope
    int scopeDepth; // number of scopes enclosing current scope
    int continueTarget;
    int breakTarget;
    int breakallTarget;
    int loopDepth;
    size_t capacity;
    MutableTable_t localIsFinals;
} Compiler_t;

// GLOBALS (uh-oh!!)
Parser_t parser;
Compiler_t *current;
Chunk_t *compilingChunk;
Table_t literals;
bool isFinal = false;

static void errorAt(Token_t *token, const char *msg) {
    if (parser.panicMode) return;   // suppress follow-on errors while in panic mode
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end.");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, " : %s\n", msg);
    fflush(stderr);
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

static void emitVarLenInstr(unsigned idx, uint8_t shortOp, uint8_t longOp) {
    if (idx < OP_SHORT_MAX) {
        emitBytes(shortOp, (uint8_t)idx);
    } else {
        emitByte(longOp);
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

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    // Placeholder
    emitByte(0xFF);
    emitByte(0xFF);
    return currentChunk()->count - 2;    // index of first byte of placeholder
}

static void patchJump(int offset) {
    // -2 to adjust bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;
    
    if (jump > UINT16_MAX) {
        error("Too much code to jump over :(.");
    }
    currentChunk()->code[offset] = (jump >> 8) & 0xFF;  // write high byte of jump
    currentChunk()->code[offset + 1] = jump & 0xFF;     // write low byte of jump
}

static void endCompiler(void) {
    FREE_ARRAY(Local_t, current->locals, current->capacity);
    freeIsFinalsArray(&current->localIsFinals);
    emitReturn();
}

static void emitConstant(Value_t value) {
    if (currentChunk()->constants.capacity > OP_LONG_MAX) {
        error("Too many constants in one chunk.");
        return;
    }

    /**
     * Uniquify values that exist in constant pool.
     * Below logic turns constant pool into a set.
     */
    Value_t idxVal;
    int constantIdx;
    if (!tableGet(&literals, value, &idxVal)) {
        constantIdx = addConstant(currentChunk(), value);
        tableSet(&literals, value, NUMBER_VAL((double)constantIdx));
    } else {
        constantIdx = (int)AS_NUMBER(idxVal);
    }

    if (constantIdx > UINT8_MAX) {
        emitLongConstant(constantIdx);
        return;
    }
    emitBytes(OP_CONSTANT, (uint8_t)constantIdx);
}

static void initLocals(void) {
    size_t oldCapacity = current->capacity;
    current->capacity = GROW_CAPACITY(oldCapacity);
    current->locals = GROW_ARRAY(Local_t, current->locals, oldCapacity, current->capacity);
    for (unsigned i=0; i < current->capacity; ++i) {
        current->locals[i].depth = -1;
    }
    initIsFinalsArray(&current->localIsFinals);
}

static void initCompiler(Compiler_t *compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->continueTarget = -1;
    compiler->breakTarget = -1;
    compiler->breakallTarget = -1;
    compiler->loopDepth = 0;
    current = compiler;
    initLocals();
}

static void expression(void);
static void parsePrecedence(Precedence_t precedence);
static ParseRule_t *getRule(TokenType_e type);

static void expression(void) {
    parsePrecedence(PREC_COMMA);
}

static void number(bool canAssign) {
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

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(makeString(parser.previous.start + 1, 
                                     parser.previous.length - 2)));
}

static bool identifiersEqual(Token_t *a, Token_t *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler_t *compiler, Token_t *name) {
    // Important that we walk back to preserve expected shadowing semantics
    for (int i=compiler->localCount - 1; i>=0; --i) {
        Local_t *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Cannot read local variable in its own initializer");
                return -1;
            }
            return i;
        }
    }
    return -1;
}

static unsigned identifierConstant(Token_t *identifier, bool isFinal);
static void namedVariable(Token_t name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg;
    unsigned idx;
    bool isLocal = (arg = resolveLocal(current, &name)) != -1; 
    if (isLocal) {
        idx = (unsigned)arg;
        getOp = OP_ACCESS_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        idx = identifierConstant(&name, false);   // OLD COMMENT: <- all we care about is providing the correct string key to tableGet(); doesn't matter if it's a copy as long as chars are same
        getOp = OP_ACCESS_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        if (isLocal && isLocalFinal(&current->localIsFinals, idx)) { 
            error("Cannot assign to 'final' variable.");
            return;
        }
        expression();
        emitVarLenInstr(idx, setOp, setOp == OP_SET_LOCAL ? OP_SET_LOCAL_LONG : OP_SET_GLOBAL_LONG);
    } else {
        emitVarLenInstr(idx, getOp, getOp == OP_ACCESS_LOCAL ? OP_ACCESS_LOCAL_LONG : OP_ACCESS_GLOBAL_LONG);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void comma(bool canAssign) {
    expression();
    consume(TOKEN_COMMA, "Expect ',' after expression.");
}

static void unary(bool canAssign) {
    TokenType_e operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        default: return;    // unreachable
    }
}

static void and_(bool canAssign) {
    /**
     * Why don't we add an 'OP_AND' instruction?
     * 
     * We could - but that'd defeat the purpose of short-circuiting.
     * Here's how the below works:
     *  case 0: false AND dontcare
     *      there's a false currently at top of stack
     *      OP_JUMP_IF_FALSE will see LHS produced false value and jump past RHS expression
     *  case 1: true AND false
     *      OP_JUMP_IF_FALSE will see LHS produced true and fallthrough
     *      OP_POP will pop 'true' from top of stack
     *      RHS will be evaluated:
     *          RHS evals to false; therefore, result of AND operation is false - so false at top of stack from RHS eval is accurate
     *  case 2: true AND true
     *      OP_JUMP_IF_FALSE will see LHS produced true and fallthrough
     *      OP_POP will pop 'true' from top of stack
     *      RHS will be evaluated:
     *          RHS evals to true; therefore, result of AND operation is true - so true at top of stack from RHS eval is accurate
     */

    // LHS has already been compiled (result at top of stack)
    int lhsFalse = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);  // evaluate RHS
    patchJump(lhsFalse);
}

static void or_(bool canAssign) {
    /**
     * Why don't we add an 'OP_OR' instruction?
     * 
     * We could - but that'd defeat the purpose of short-circuiting.
     * Here's how the below works:
     *  case 0: true OR dontcare
     *      there's a true currently at top of stack
     *      OP_JUMP_IF_TRUE will see LHS produced true value and jump past RHS expression
     *  case 1: false OR false
     *      OP_JUMP_IF_TRUE will see LHS produced false and fallthrough
     *      OP_POP will pop 'false' from top of stack
     *      RHS will be evaluated:
     *          RHS evals to false; therefore, result of OR operation is false - so false at top of stack from RHS eval is accurate
     *  case 2: false OR true
     *      OP_JUMP_IF_TRUE will see LHS produced false and fallthrough
     *      OP_POP will pop 'false' from top of stack
     *      RHS will be evaluated:
     *          RHS evals to true; therefore, result of OR operation is true - so true at top of stack from RHS eval is accurate
     */

    // LHS has already been compiled (result at top of stack)
    int lhsTrue = emitJump(OP_JUMP_IF_TRUE);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);  // evaluate RHS
    patchJump(lhsTrue);

    /**
     * ^I did the above like this for future me's readability, but the "efficient" approach
     *  would be to implement short-circuiting for both OR and AND using the same OP_JUMP_IF_FALSE op.
     *  This is what that'd look like
     * 
     *  int lhsFalse = emitJump(OP_JUMP_IF_FALSE);
     *  int lhsTrue = emitJump(OP_JUMP);    // can only get here when LHS is true
     * 
     *  patchJump(lhsFalse);    // if LHS was false, short jump over here
     *  emitByte(OP_POP);   // pop false from stack
     *  parsePrecedence(PREC_OR);   // evaluate RHS
     * 
     *  patchJump(lhsTrue); // if LHS was true, no need to eval RHS
     */
}

static void binary(bool canAssign) {
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
        case TOKEN_MODULO:          emitByte(OP_MODULO); break;
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

static void ternary(bool canAssign) {
    // expression was just evaluated - result at top of stack
    int falseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);   // pop true
    expression();   // true branch
    int exitJump = emitJump(OP_JUMP);
    consume(TOKEN_COLON, "Expected a ':'.");
    patchJump(falseJump);
    emitByte(OP_POP);  // pop false
    expression();   // false branch
    patchJump(exitJump);
}

static void literal(bool canAssign) {
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
    [TOKEN_COMMA]           =  {NULL, NULL, PREC_COMMA},    // TODO: implement prefix
    [TOKEN_EQUAL]           =  {NULL, NULL, PREC_ASSIGNMENT},
    [TOKEN_DOT]             =  {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS]           =  {unary, binary, PREC_TERM},
    [TOKEN_PLUS]            =  {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON]       =  {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH]           =  {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR]            =  {NULL, binary, PREC_FACTOR},
    [TOKEN_MODULO]          =  {NULL, binary, PREC_FACTOR},
    [TOKEN_QUESTION_MARK]   =  {NULL, ternary, PREC_TERNARY},
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
    [TOKEN_AND]             =  {NULL, and_, PREC_AND},
    [TOKEN_OR]              =  {NULL, or_, PREC_OR},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        // TODO: Remove this and uncomment below
        //      - need to find way to reach below with
        //        canAssign == true && match(TOKEN_EQUAL)
        //       test case: a * b = 3;
        if (canAssign && match(TOKEN_EQUAL)) {
            error("Invalid assignment target.");
            return;
        }
        /**
         * If we're in the middle of parsing an expression and we hit a 
         * ',' it means we're hitting a sequence point and we're through
         * parsing this expression
         */
        if (check(TOKEN_COMMA)) break;
        advance();
        ParseFn_t infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    // if (canAssign && match(TOKEN_EQUAL)) {
    //     error("Invalid assignment target.");
    // }

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
    expression();   // allows for print(a) or print a b/c () in print(a) are a grouping
    if (!match(TOKEN_COMMA))
        consume(TOKEN_SEMICOLON, "Expected a ';'.");
    emitByte(OP_PRINT);
}

static void expressionStatement(void) {
    expression();
    if (!match(TOKEN_COMMA))
        consume(TOKEN_SEMICOLON, "Expected a ';'.");
    emitByte(OP_POP);
}

static unsigned makeConstant(Value_t value) {
    int idx = addConstant(currentChunk(), value);
    if (idx >= OP_LONG_MAX) {
        fprintf(stderr, "Constant pool is too large.");
        exit(EXIT_FAILURE);
    }

    return (unsigned)idx;
}

static void markInitialized(void) {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static unsigned identifierConstant(Token_t *identifier, bool isFinal) {
    Value_t idxVal;
    unsigned idx;
    Value_t key = OBJ_VAL(makeString(identifier->start, identifier->length));
    if (tableGet(&vm.globalNames, key, &idxVal)) {
        // TODO: fix memory leak here... if key already exists in table, it should be freed
        idx = (unsigned)AS_NUMBER(idxVal);
    } else {
        idx = (unsigned)vm.globalValues.count;
        tableSet(&vm.globalNames, key, NUMBER_VAL((double)idx));
        writeValueArray(&vm.globalValues, UNDEFINED_VAL);
        writeIsFinalsArray(&vm.globalIsFinals, isFinal);
    }
    return idx;
}

static void addLocal(Token_t *name) {
    if (current->localCount == OP_LONG_MAX) {
        error("Too many local variables in a function.");
        return;
    }
    if (current->capacity < current->localCount + 1) {
        size_t oldCapacity = current->capacity;
        current->capacity = GROW_CAPACITY(oldCapacity);
        current->locals = GROW_ARRAY(Local_t, current->locals, oldCapacity, current->capacity);

        // Initialize 'unused' locals to depth -1
        for (unsigned i=oldCapacity; i<current->capacity; ++i) {
            current->locals[i].depth = -1;
        }
    }

    Local_t *local = &current->locals[current->localCount++];
    local->name = *name;
    writeIsFinalsArray(&current->localIsFinals, isFinal);

    // NOTE: below is necessary! ...but already handled in initLocals :)
    // local->depth = -1;   // sentinel value indicating var is declared but not yet *defined*
}

static void declareVariable(void) {
    // REMINDER: global vars are late bound
    //           local vars are bound at compile time
    if (current->scopeDepth == 0) return;
    Token_t *name = &parser.previous;

    /**
     * Check:
     *      Avoid the following:
     *          {
     *              var a = 2;
     *              var a = 3;
     *          }
     * 
     *      I'm worried that we'll still run into the following false positive:
     *          {
     *              {
     *                  var a = 2;
     *              }
     *              {
     *                  var a = 3;
     *              }
     *          }
     * 
     *      aha! but here's something to ALWAYS keep in mind...
     *      current->locals contains an array of locals that *must*
     *      always be monotonically increasing in depth.
     *      
     *      why? because as soon a var goes out of scope (endScope is called)
     *      it's popped off the stack and removed from locals :)
     */
    for (int i=current->localCount - 1; i >= 0; --i) {
        Local_t *local = &current->locals[i];
        if (local->depth != -1 &&
            local->depth < current->scopeDepth) {
                break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Cannot re-declare var in same scope.");
            return;
        }
    }

    addLocal(name);
}

static unsigned parseVariableName(const char *errMsg) {
    consume(TOKEN_IDENTIFIER, "Expect an indentifier");
    declareVariable();
    if (current->scopeDepth > 0) return 0;
    return identifierConstant(&parser.previous, isFinal);
}

static void defineVariable(unsigned global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitVarLenInstr(global, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}

static void varDeclaration(void) {
    unsigned global = parseVariableName("Expect a variable name");
    // TODO: Add flag to avoid going here twice (i.e. for *new* globals, this is unnecessary)
    if (current->scopeDepth == 0) {
        // Update isFinal for globals
        // Necessary for re-declaration of globals
        writeIsFinalsArrayAt(&vm.globalIsFinals, isFinal, global);
    }

    if (match(TOKEN_EQUAL)) {
        expression();       // <- a value will be pushed to stack
    } else {
        emitByte(OP_NIL);   // <- NULL will be pushed to stack
    }

    consume(TOKEN_SEMICOLON, "Expect a ';'.");
    defineVariable(global);
}

static void beginLoop(void) {
    current->loopDepth++;
}

static void endLoop(void) {
    current->loopDepth--;
}

static void beginScope(void) {
    current->scopeDepth++;
}

static void endScope(void) {
    current->scopeDepth--;

    // TODO: replace with OP_POPN
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
        popLocalIsFinalFlag(&current->localIsFinals);
    }
}

static void declaration(void);
static void block(void) {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect a '}' at end of block.");
}

static void declaration(void);
static void ifStatement(void) {
    consume(TOKEN_LEFT_PAREN, "Expected a '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected a ')' after condition.");
    
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);   // pop result of condition in "consition is true" case
    declaration();
    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);   // if condition is false, control jumps here - time to clean up the stack

    if (match(TOKEN_ELSE)) declaration();
    patchJump(elseJump);
}

// static int emitLabel(uint8_t label) {
//     emitByte(label);
//     return currentChunk()->count - 1;
// }

// static void emitBackJump(int dest) {
//     uint16_t dst = (uint16_t)dest;
//     emitByte(OP_JUMP_BACK);
//     emitBytes((dest >> 8) & 0xFF, dest & 0xFF);
// }

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    // Emit offset indicating how far to jump back
    //  +2 for to account for operand bytes emitted below
    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emitByte((offset >> 8) & 0xFF);
    emitByte(offset & 0xFF);
}

static void continueStatement(void) {
    consume(TOKEN_SEMICOLON, "Expect a ';'.");
    if (current->loopDepth <= 0) {
        error("Cannot use 'continue' outside of a loop.");
        return;
    }
    emitLoop(current->continueTarget);
}

static void breakStatement(void) {
    consume(TOKEN_SEMICOLON, "Expect a ';'.");
    if (current->loopDepth <= 0) {
        // TODO: Modify when switch is added
        error("Cannot use 'break' outside of loop.");
        return;
    }
    // endLoop();  // decrement loop depth
    // endScope(); // decrement scope depth
    current->breakTarget = emitJump(OP_JUMP);
}

static void breakAllStatement(void) {
    consume(TOKEN_SEMICOLON,"Expect a ';'.");
    if (current->loopDepth <= 0) {
        error("Cannot use 'breakall' outside of loop.");
        return;
    }
    current->breakallTarget = emitJump(OP_JUMP);
}

static void whileStatement(void) {
    // Book solution
    int breakTarget = current->breakTarget;
    int continueTarget = current->continueTarget;
    beginLoop();
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expected a '(' after 'while'.");
    int loopStart = currentChunk()->count;
    current->continueTarget = loopStart;
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected a ')' after condition.");
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    declaration();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);   // pop result of condition evaluation
    if (current->breakTarget != -1) patchJump(current->breakTarget);
    if (current->breakallTarget != -1 && current->loopDepth == 1) {
        patchJump(current->breakallTarget);
        current->breakallTarget = -1;   // reset 'breakall' flag
    }
    endScope();
    endLoop();
    current->breakTarget = breakTarget;
    current->continueTarget = continueTarget;

    // BELOW IS FIRST ATTEMPT
    // consume(TOKEN_LEFT_PAREN, "Expected a '(' after 'while'.");
    // int label = emitLabel(OP_LOOP);
    // expression();
    // consume(TOKEN_RIGHT_PAREN, "Expected a ')' after condition.");

    // int bodyJump = emitJump(OP_JUMP_IF_FALSE);
    // emitByte(OP_POP);
    // statement();
    // emitBackJump(label);   // TODO: jump backwards
    // patchJump(bodyJump);
    // emitByte(OP_POP);
}

static void forStatement(void) {
    beginLoop();
    beginScope();   // for statements initiate a new scope
    consume(TOKEN_LEFT_PAREN, "Expect a '(' after 'for'.");
    if (!match(TOKEN_SEMICOLON)) {
        // Initializations may be empty
        if (match(TOKEN_VAR)) {
            varDeclaration();
        } else {
            // expression();
            // emitByte(OP_POP);
            // consume(TOKEN_SEMICOLON, "Expect a ';' after initialization.");
            expressionStatement();
        }
    }

    int condStart = currentChunk()->count;
    if (!match(TOKEN_SEMICOLON)) {
        // Conditions may be empty
        expression();
        consume(TOKEN_SEMICOLON, "Expect a ';' after condition.");
    } else {
        /**
         * for (;;) { ... } <- should be an infinite loop
         */
        emitByte(OP_TRUE);
    }
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);   // Condition is true
    int bodyJump = emitJump(OP_JUMP);

    int updateStart = currentChunk()->count;
    current->continueTarget = updateStart;
    if (!match(TOKEN_RIGHT_PAREN)) {
        expression();
        emitByte(OP_POP);   // Update
        consume(TOKEN_RIGHT_PAREN, "Expect a ')' after update.");
    }
    emitLoop(condStart);  // update  ->  condition

    patchJump(bodyJump);
    declaration();
    emitLoop(updateStart);  // body  ->  update
    patchJump(exitJump);
    current->breakTarget = currentChunk()->count;
    emitByte(OP_POP);  // Condition is false
    endScope();
    endLoop();
}

/**
 * LOX GRAMMAR RULES:
 *  **NOTE:** This is a little more bare-bones than the jlox grammar doc comment.
 *            The clox and jlox grammars are still identical - I'm just adding 
 *            grammar rules here to align more closely with the code you're reading.
 *            It's easier to write the rules next to the code in a recursive descent
 *            parser (jlox) than it is here with a Pratt Parser (clox).
 * 
 *  declaration      ->  varDecl | ifStmt | whileStmt | stmt ;
 *  varDecl          ->  "var" IDENTIFIER ("=" expression)? ";" ;
 *  ifStmt           ->  "if" "(" condition ")" block ";" ;
 *  whileStmt        ->  "while" "(" condition ")" statement ;
 *  forStmt          ->  "for"   "(" initialization ";" condition ";" update ")" statement ;
 *  initialization   ->  expression ;
 *  condition        ->  expression ;
 *  update           ->  expression ;
 *  stmt             ->  printStmt | exprStmt | block ;
 *  printStmt        ->  "print" "(" expression ")" ";" ;
 *  exprStmt         ->  expression ";" ;
 *  block            ->  "{" declaration* "}"
 *     
 *  expression       -> ... TODO: Complete me :)
 */

static void statement(void) {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else if (match(TOKEN_BREAK)) {
        breakStatement();
    } else if (match(TOKEN_BREAKALL)) {
        breakAllStatement();
    }else if (match(TOKEN_CONTINUE)) {
        continueStatement();
    } else {
        expressionStatement();
    }
}

static void declaration(void) {
    if (match(TOKEN_FINAL)) {
        isFinal = true;
        consume(TOKEN_VAR, "Expected 'var'.");
        varDeclaration();
        isFinal = false;
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
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
    Compiler_t compiler = { 0 };
    initCompiler(&compiler);
    // initTable(&vm.globalNames);
    initTable(&literals);

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

    // freeTable(&vm.globalNames);
    freeTable(&literals);
    endCompiler();

    #ifdef DEBUG_CHUNK
    #include "debug.h"
    disassembleChunk(currentChunk(), "code");
    #endif

    return !parser.hadError;
}