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

static char advance(void) {
    return *scanner.current++;
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current == expected) {
        scanner.current++;
        return true;
    }
    return false;
}

static char peek(void) {
    return *scanner.current;
}

static char peekNext(void) {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static void skipMultiLineComment(void) {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case '/':
                if (peekNext() == '*') {
                    advance(); advance();
                    skipMultiLineComment();
                }
                break;
            case '*':
                if (peekNext() == '/') {
                    advance(); advance();
                    return;
                }
                break;
            case '\n':
                scanner.line++;
                break;
        }
        advance();
    }
}

static void skipWhiteSpace(void) {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() == '*') {
                    advance(); advance();
                    skipMultiLineComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token_t string(void) {
    char c;
    while (!isAtEnd() && (c = peek()) != '"') {
        if (c == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance();  // consume "
    return makeToken(TOKEN_STRING);
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static Token_t number(void) {
    char c;
    while (!isAtEnd() && isDigit(c = peek())) advance();

    if (c == '.') {
        advance();  // consume '.'
        while (!isAtEnd() && isDigit(c = peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

Token_t scanToken(void) {
    skipWhiteSpace();
    scanner.start = scanner.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);
    char c = advance();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_BRACK);
        case ']': return makeToken(TOKEN_RIGHT_BRACK);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '%': return makeToken(TOKEN_MODULO);
        case '?': return makeToken(TOKEN_QUESTION_MARK);
        case ':': return makeToken(TOKEN_COLON);
        case '&': return makeToken(TOKEN_BITWISE_AND);
        case '|': return makeToken(TOKEN_BITWISE_OR);
        case '^': return makeToken(TOKEN_BITWISE_XOR);
        case '/': return makeToken(TOKEN_SLASH);
        case '+': 
            return makeToken(match('+') ? TOKEN_PLUS_PLUS : TOKEN_PLUS);
        case '-': 
            return makeToken(match('-') ? TOKEN_MINUS_MINUS : TOKEN_MINUS);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : 
                             match('>') ? TOKEN_BITSHIFT_RIGHT : TOKEN_GREATER);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : 
                             match('<') ? TOKEN_BITSHIFT_LEFT : TOKEN_LESS);
        case '*':
            return makeToken(match('*') ? TOKEN_STAR_STAR : TOKEN_STAR);
        
        case '"':
            return string();

        default:

    }

    return errorToken("Unexpected character.");
}
