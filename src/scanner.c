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

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static TokenType_e checkKeyword(unsigned short startIdx, size_t substrLen, 
    const char *substr, TokenType_e type) {
    // char *curr = &scanner.start[startIdx];
    // for (unsigned i=0; i<substrLen; ++i) {
    //     if (*curr != substr[i]) return TOKEN_IDENTIFIER;
    //     curr++;
    // }
    // return type;

    if (scanner.current - scanner.start == startIdx + substrLen &&
        memcmp(scanner.start + startIdx, substr, substrLen) == 0) {
            return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType_e identifierType(void) {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
        case 'c': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'l':
                        return checkKeyword(1, 4, "lass", TOKEN_CLASS);
                    case 'o':
                        return checkKeyword(1, 7, "ontinue", TOKEN_CONTINUE);
                }
            }
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(1, 4, "alse", TOKEN_FALSE);
                    case 'o':
                        if (scanner.current - scanner.start > 3) {
                            return checkKeyword(1, 6, "oreach", TOKEN_FOREACH);
                        } else {
                            return checkKeyword(1, 2, "or", TOKEN_FOR);
                        }
                    case 'u': return checkKeyword(1, 2, "un", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(1, 3, "his", TOKEN_THIS);
                    case 'r': return checkKeyword(1, 3, "rue", TOKEN_TRUE);
                }
            }
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token_t identifier(void) {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
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
    if (isAlpha(c)) return identifier();
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
