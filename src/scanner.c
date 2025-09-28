#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

Scanner_t scanner;

void initScanner(const char *source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static Token_t makeToken(TokenType_e type) {
    Token_t token;
    token.type = type;
    token.start = scanner.start;
    token.length = (size_t)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token_t errorToken(const char *msg) {
    Token_t token;
    token.type = TOKEN_ERROR;
    token.start = msg;
    token.length = strlen(msg);
    token.line = scanner.line;
    return token;
}

static bool isAtEnd(void) {
    return *scanner.current == '\0';
}

Token_t scanToken(void) {
    return makeToken(TOKEN_EOF);
    scanner.start = scanner.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    return errorToken("Unexpected character.");
}
