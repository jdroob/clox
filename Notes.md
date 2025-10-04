# Getting started
## Dev log
- 8/14/2025
    - Woo! Back to C :)

- 10/3/2025
    - Been a while lol
    - Got back from Europe in mid-September
    - Finally finished scanner
    - Here are some thoughts on scanning:
        - Remember the general patterns:
        1) Read the input script & transform to a string
        ```C
        FILE *file = fopen(filename, "rb");
        ...
        size_t bytesRead = fread(buffer, fileSize, file);
        buffer[bytesRead] = '\0';
        ```
        2) Implement a function to drive the scanning. In this case, 'compile'.
            This function repeatedly calls the 'scanToken' function until the 
            string has been completely tokenized.
        ```C
        // compiler.c
        void compile(const char *source) {
            initScanner(source);
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
        }
        ```
        3) Implement 'scanToken' and it's helpers. On each match, the global state of the scanner is updated. Specifically, scanner.start is bumped to the next input character. scanner.current is updated on each consumed character (each time advance is called).
        ```C
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
                }

                return errorToken("Unexpected character.");
            }
        ```
    - Implementation Note:
        - In jlox, we maintained a list of all scanned tokens after we were finished scanning. In clox, this will not be the case - as we'll see in future chapters. In fact, at the time of writing, all we're doing is scanning a token and printing it. The lifetime of the token is a single turn around the loop in compile. Later, we'll maintain about two tokens at a time and use those during parsing and bytecode generation.
        - Notice what a token is in clox vs jlox. In jlox, a token maintains: it's own copy of the input lexeme, an Object-type value for literals (e.g., 42 for input literal "42"), as well as line number and enum type (like clox). Using this same data layout in clox would be cumbersome. We'd need to malloc space for the lexeme and literal fields. We'd do this for each token. And we'd need to use a memory-safe approach so we don't end up leaking memory. This is certainly doable - but unnecessary. Instead, we can just use a good 'ol char * pointer to point to the start of the token and a size_t len member to track how many characters are in the token. This approach requires no dynamic memory allocation and makes my life easier as a programmer :)


    - Challenges:
        1) Many newer languages support string interpolation. Inside a string
            literal, you have some sort of special delimiters—most commonly ${
            at the beginning and } at the end. Between those delimiters, any
            expression can appear. When the string literal is executed, the inner
            expression is evaluated, converted to a string, and then merged with
            the surrounding string literal.
            For example, if Lox supported string interpolation, then this . . .
            ```
            var drink = "Tea";
            var steep = 4;
            var cool = 2;
            print "${drink} will be ready in ${steep + cool} minutes.";
            ```
             . . . would print:
            ```
            Tea will be ready in 6 minutes.
            ```
            What token types would you define to implement a scanner for string
            interpolation? What sequence of tokens would you emit for the above
            string literal?
            What tokens would you emit for:
            ```
            "Nested ${"interpolation?! Are you ${"mad?!"}"}"
            ```
            Consider looking at other language implementations that support
            interpolation to see how they handle it.

        Answer:
            Looks like Python doesn't treat f-strings differently from raw strings at scanning stage:
            ```shell
            $ echo "x = 42; print(f'x: {x}')" | python3 -m tokenize
                1,0-1,1:            NAME           'x'
                1,2-1,3:            OP             '='
                1,4-1,6:            NUMBER         '42'
                1,6-1,7:            OP             ';'
                1,8-1,13:           NAME           'print'
                1,13-1,14:          OP             '('
                1,14-1,23:          STRING         "f'x: {x}'"
                1,23-1,24:          OP             ')'
                1,24-1,25:          NEWLINE        '\n'
                2,0-2,0:            ENDMARKER      ''
                $ echo "x = 42; print('hi')" | python3 -m tokenize
                1,0-1,1:            NAME           'x'
                1,2-1,3:            OP             '='
                1,4-1,6:            NUMBER         '42'
                1,6-1,7:            OP             ';'
                1,8-1,13:           NAME           'print'
                1,13-1,14:          OP             '('
                1,14-1,18:          STRING         "'hi'"
                1,18-1,19:          OP             ')'
                1,19-1,20:          NEWLINE        '\n'
                2,0-2,0:            ENDMARKER      ''
            ```