#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

#include "common.h"

typedef enum {
    // Single-character tokens
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, 
    TOKEN_RIGHT_BRACE, TOKEN_LEFT_BRACK, TOKEN_RIGHT_BRACK,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS, 
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR, 
    TOKEN_MODULO, TOKEN_QUESTION_MARK, TOKEN_COLON,
    TOKEN_BITWISE_AND, TOKEN_BITWISE_OR, TOKEN_BITWISE_XOR, 

    // One or two character tokens
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_BITSHIFT_LEFT, TOKEN_BITSHIFT_RIGHT,
    TOKEN_PLUS_PLUS, TOKEN_MINUS_MINUS,
    TOKEN_STAR_STAR,

    // Literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INTERPOLATION, TOKEN_NUMBER,

    // Keywords
    TOKEN_AND, TOKEN_BREAK, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FUN, TOKEN_FOR, TOKEN_FOREACH, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE, TOKEN_CONTINUE,

    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NOT_FOUND
} TokenType_e;

typedef struct {
    const char *start;
    const char *current;
    int line;
    int interpolationState;
    int stringNestingLevel;
} Scanner_t;

typedef struct {
    TokenType_e type;
    const char *start;
    int length;
    int line;
} Token_t;

void initScanner(const char *source);
Token_t scanToken(void);

#endif // CLOX_SCANNER_H