# Getting started
## Dev log
- 8/14/2025
    - Woo! Back to C :)

## Chapter 17: Scanning
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

        2) List<List<T>> l; - how does C++ parse this correctly? IOW, how does it know the '>>' are closing angle brackets to the type arguments?

        Answer:
        
        Based on [this](https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter16_scanning.md#2), apparently C++ activates a rule when a `<` is scanned. Once a `<` is scanned, `>>` are treated as closing angle brackets rather than a bitshift right operator. My guess is while this is how it's *scanned*, the parser can eventually learn that `>>` should be treated as a bitshift right rather than two `>` tokens.


        3) How do you implement contextual keywords?

        Answer:
        
        Just scan the keyword as an identifier (i.e., assume it's in "identifier mode" until proven otherwise). Then, when parsing, determine if we're parsing *that* context. If so, the parser needs to check the scanned identifier to determine if the identifier matches contextual keyword (that can appear in that position in the syntactic grammar). If so, treat it as a contextual keyword instead of an identifier.

    ## Chapter 17: Compiling Expressions 
    - TODO: how Pratt parsing works

    1) To really understand the parser, you need to see how execution threads through the interesting parsing functions—parsePrecedence() and the parser functions stored in the table. Take this (strange) expression:

    `(-1 + 2) * 3 - -4`

    Write a trace of how those functions are called. Show the order they are called, which calls which, and the arguments passed to them.<br>
    Answer:
    ```
    expression()
        parsePrecedence(PREC_COMMA)
            grouping()  // for '('
                expression()
                    parsePrecedence(PREC_COMMA)
                        unary() // for '-'
                            parsePrecedence(PREC_UNARY)
                                number()    // for '1'
                                    emitConstant(1)
                        emitByte(OP_NEGATE)
                        binary()    // for '+'
                            parsePrecedence(PREC_FACTOR)    // PREC_TERM + 1
                                number()    // for '2'
                                    emitConstant(2)
                            emitByte(OP_ADD)
        /* at this point: parser.prev = '*', parser.current = '3' */
            binary()    // for '*'
                parsePrecedence(PREC_UNARY) // PREC_FACTOR + 1
                    number()    // for '3'
                        emitConstant(3)
                emitByte(OP_MULTIPLY)
        /* at this point: parser.prev = '-', parser.current = '-' */
            binary()    // for first '-' in 3 - -4
                parsePrecedence(PREC_FACTOR)    // PREC_TERM + 1
                    unary() // for '-' in -4
                        parsePrecedence(PREC_UNARY)
                            number()    // for '4'
                                emitConstant(4)
                        emitByte(OP_NEGATE)
                emitByte(OP_SUBTRACT)
    ```

    2) The ParseRule row for TOKEN_MINUS has both prefix and infix function pointers. That’s because - is both a prefix operator (unary negation) and an infix one (subtraction).
    
    In the full Lox language, what other tokens can be used in both prefix and infix positions? What about in C or in another language of your choice?<br>
    Answer:
    In Lox, there will be rows with both prefix and infix rules for the same tokens in the following cases:
        - `TOKEN_LEFT_PAREN`: for grouping and function call
        - `TOKEN_LEFT_BRACK`: for array literal and indexing

    I think that's it? In C there's obviously '*' for dereferencing and multiplication. And '&' for 'address-of' and bitwise AND.

    3) You might be wondering about complex “mixfix” expressions that have more than two operands separated by tokens. C’s conditional or “ternary” operator, ?:, is a widely known one. Add support for that operator to the compiler. You don’t have to generate any bytecode, just show how you would hook it up to the parser and handle the operands.<br>
    Answer:
        Donezo

    ## Chapter 18: Types of Values 


    ## Hash table & Global Variables
    - tombstone: when we "delete" an entry from a hash table,
        we set the key to EMPTY and value to 'true' like so:

        ```
         // Tombstone
        entry->key = EMPTY_VAL;
        entry->value = BOOL_VAL(true);
        ```

        We also DON'T decrement table->count.
        The reason for this is we want tombstones to count as
        an a "non-empty" entry during linear probing. 
        Linear probing stops when a true empty slot is found.
        If a tombstone was found along the way, we use that instead.
        (re-using tombstones is efficient! :))
        This implies that we shouldn't increase the count when a tombstone is found (b/c we never decremented the count when we tombstonified the entry).

        The "con" to not decreementing count is that we need to re-size the array more frequntly. The "pro" is linear probing doesn't break b/c we'll always have empty slots :)

    11/29/2025:
        BUG:
            - in table.c::adjustCapacity(), the idea is iterate through ALL
              entries in ORIGINAL table and copy over non-empty entries to 
              appropriate index in NEW table
            - The NUMBER OF TIMES I was iterating was lining up with the new
              table (bigger) instead of the old table
            - IOW - an INDEX OUT OF BOUND ERROR!

        NOTE:
            - Today I learned, the constant pool is a pool for constants :) lol
            - No but seriously, consider:
                ```
                    var a = "john";
                    print(a);   // john
                ```
            - These statements yields the bytecode sequence:
                OP_CONSTANT  1  ; string "john" stored in const pool at idx 1
                OP_DEFINE_GLOBAL 0 ; string "a" stored in const pool at idx 0
                OP_ACCESS_GLOBAL 2 ; duplicate string "a" stored in const pool at idx 2; used as key to push value to stack
                OP_PRINT

            - A key insight here is: rather than iterating through the constant table to find the index of the constant array with the value "a" (just so we can use that as a key) - we just add another copy of the string "a" to the constant pool and use it as a key when needed. The former approach could really slow down the interpreter (O(N) for each variable usage??). The purpose of the constants in the constant pool are simply to use the values encoded in the source program when it's time to use them. Uniqueness does not matter here - we're not using maps - just a list of values.

12/5/2025:
    - Recap of how global vars are stored and retrieved in clox:
        - Consider the code: `var a = 2; print(a);`
        - When compiling:
            - 'a' is added to the constant table (index 0) as part of an OP_DEFINE_GLOBAL instr
            - 2 is added to the constant table (index 1) as part of an OP_CONSTANT instr
            - An OP_SET_GLOBAL instr is emitted:
                - A separate copy of 'a' is added to the constant table (index 2)
                - Semantically, this instr sets the variable with string 'a' (in globals table)
                  to value at top of value stack (in this case 2).

        - See an opportunity for optimization? Why are we adding a new copy of 'a' to the value stack
          for the OP_SET_GLOBAL instr? We already have an 'a' at index 0? Seems wasteful... Particularly,
          when you imagine Lox programs with MANY MANY reads and writes of the same variable... Think
          of a for loop with 1000 iterations. Each iteration, 'i' is tested, and 'x' is updated. Using our
          current approach, this would result in the constant pool containing 1000 copies of 'i' and 1000 copies
          of 'x' just for that for loop! So wasteful! (Yes, we now have 2^24 slots for constants, but what if there were 1,000,000 iterations? :/ ).

        - Solution? Before adding a new constant to the constant pool, why not check if the same value has been added recently? Yes, this incurs a compile-time cost of reading back through the constant pool (N number of slots). And if the value to be added does not already exist in the constant pool, it's added. Otherwise, the index of the existing value is returned. Seems tedious to do this on each write, but if we keep N small-ish, it's no big deal. Still O(c * 1) - just a slightly bigger c.

        Here's the code from `addConstant` in value.c

        ```
        ...
        // Are we just re-writing a value that's recently been written??
        if (array->count > 0) {
            for (int idx=array->count - 1; idx >= 0 && idx > idx - MRU_SL; --idx) {
                Value_t candidate = array->values[idx];
                if (valuesEqual(candidate, value)) {
                    // If so... don't do that!
                    return (unsigned)idx;
                }
            }
        }
        ...
        ```

        And here's the bytecode for the Lox code: 
            `var a; var b; a=2, b=1, print(a), print(b);`

        ```
        ==code==
        0000 0022 OP_NIL
        0001  | OP_DEFINE_GLOBAL    0 '"a"
        '
        0003  | OP_NIL
        0004  | OP_DEFINE_GLOBAL    1 '"b"
        '
        0006 0023 OP_CONSTANT         2 '2
        '
        0008  | OP_SET_GLOBAL       0 '"a"
        '
        0010  | OP_POP
        0011  | OP_ONE
        0012  | OP_SET_GLOBAL       1 '"b"
        '
        0014  | OP_POP
        0015  | OP_ACCESS_GLOBAL    0 '"a"
        '
        0017  | OP_PRINT
        0018  | OP_ACCESS_GLOBAL    1 '"b"
        '
        0020  | OP_PRINT
        0021 0028 OP_RETURN
        ```

    - Notice how 'a' is originally written to constant pool index 0, 'b' to 1.
      Then, when setting 'a', 'a' is retrieved from index 0 (and 'b' from 1).
    
    - Using this approach, we can keep the constant pool small :)

    - Future improvements? Hash-based constant-pool. Probs no big deal to have linear probing at 2 levels (constant pool + globals table). My experience has been that in large code bases, the number of identifiers increases at a rather slow rate (for well-maintained codebases).

       - Idea: we'd take a string: like 'a' or '1'. We'd hash it. Add the value to a ValueArray (constant pool) based off the hash. Before adding, we'd search to see if it's already in there. If so, don't add just return the index. Otherwise, add then return the index.

       TODO 1: Create a stringConstants table used at compile time. keys are var names. Values are indices in the constant pool. Use this to avoid re-adding values to the constant pool.

       TODO 2: Create a globals table that maps var names to indices in a globa ValueArray. At compile-time, add new vars to a globals table, map them to their index in the globals ValueArray. Emit bytecode with each set and get instr having an index operand. At runtime, each set and get will simply write values to a value array where each index is associated with a specific global variable :)

12/10/2025:
    How we flag writing / reading from undef'd global variables:

    ```
    case OP_DEFINE_GLOBAL: 
            case OP_DEFINE_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_DEFINE_GLOBAL) {
                    idx = (unsigned)READ_BYTE();  // read 1-byte index into vm.globalValues
                } else {
                    idx = READ_BYTES(); // read 3-byte index into vm.globalValues
                }
                Value_t value = pop();
                writeValueArrayAt(&vm.globalValues, value, idx);
                break;
            }
            case OP_ACCESS_GLOBAL:
            case OP_ACCESS_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_ACCESS_GLOBAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }
                Value_t value = getValueAt(&vm.globalValues, idx);
                if (IS_UNDEFINED(value)) {
                    runtimeError("Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL:
            case OP_SET_GLOBAL_LONG: {
                unsigned idx;
                if (instruction == OP_SET_GLOBAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }

                /**
                 * Like C, the expression <identifier> = <value>
                 *  produces the value <value>. Thus, <value> must be
                 *  at the top of the stack after the assignment is complete.
                 *  We *could* do something like: pop, add to table, push... but why?
                 *  Instead, just peek at entry in globals valueArray and be leave the
                 *  stack alone (since that's the net effect anyway)
                 */
                
                // Should have been set to NIL or defined value by this point
                if (IS_UNDEFINED(getValueAt(&vm.globalValues, idx))) {
                    runtimeError("Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                writeValueArrayAt(&vm.globalValues, peek(0), idx);
                break;
            }
    ```

    - Note how we check if the value at idx is UNDEFINED before reading from / writing to it (except for in var declaration)