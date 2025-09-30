#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"


static char *readFile(char *filename) {
    FILE *file = fopen(filename, "rb");

    if (!file) {
        fprintf(stderr, "Cannot open file: \"%s\"\n", filename);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)jrmalloc(fileSize + 1);
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read \"%s\"\n", filename);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Unable to read file \"%s\"\n", filename);
        exit(74);
    }
    buffer[bytesRead] = '\0';
    
    fclose(file);
    return buffer;
}

static void repl(void) {
    char line[1024] = {0};
    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
    }

    interpret(line);
}

static void runFile(char *path) {
    char *source = readFile(path);
    // #ifdef DEBUG
    // printf("source code read from readFile: %s\n", source);
    // #endif

    InterpResult_t result = interpret(source);
    jrfree(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, char *argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "usage: clox [script]\n");
        exit(75);
    }
    
    // Chunk_t chunk = {0};
    
    // writeConstant(&chunk, 1.2, 123);
    // writeConstant(&chunk, 3.4, 456);
    // writeConstant(&chunk, 5.6, 456);
    // writeConstant(&chunk, 7.8, 456);
    // writeChunk(&chunk, OP_NEGATE, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // interpret(&chunk);
    
    
    
    // writeConstant(&chunk, 9.10, 788);
    // pop();
    // interpret(&chunk);
    // writeChunk(&chunk, OP_NEGATE, 1011);
    // writeChunk(&chunk, OP_ADD, 1213);
    // writeChunk(&chunk, OP_RETURN, 789);
    // pop();
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    // writeChunk(&chunk, OP_RETURN, 789);
    
    // Let's evaluate: -((1.2 + 3.4) / 5.6)
    // initChunk(&chunk);
    // // initVM();
    // // writeConstant(&chunk, 1.2, 1);  // [1.2]
    // // writeConstant(&chunk, 3.4, 1);  // [1.2] [3.4]
    // // writeChunk(&chunk, OP_ADD, 1);  // [4.6]
    // // writeConstant(&chunk, 5.6, 2);  // [4.6] [5.6]
    // // writeChunk(&chunk, OP_DIVIDE, 2);  // [4.6 / 5.6]
    // // writeChunk(&chunk, OP_NEGATE, 3);  // [ -(4.6/5.6) ]
    // // writeChunk(&chunk, OP_RETURN, 4);  // end of expression eval
    // // interpret(&chunk);
    // // freeVM();
    // // freeChunk(&chunk);

    // // #2: 1 * 2 + 3
    // initChunk(&chunk);
    // initVM();
    // writeConstant(&chunk, 1, 1);    // [1]
    // writeConstant(&chunk, 2, 1);    // [1] [2]
    // writeChunk(&chunk, OP_MULTIPLY, 1);     // [1 * 2]
    // writeConstant(&chunk, 3, 1);    // [1 * 2] [3]
    // writeChunk(&chunk, OP_ADD, 1);  // [1 * 2 + 3]
    // writeChunk(&chunk, OP_RETURN, 1);
    // interpret(&chunk);
    // freeVM();
    // freeChunk(&chunk);

    // // #3: 1 + 2 * 3 - 4 / -5
    // initChunk(&chunk);
    // initVM();
    // writeConstant(&chunk, 1, 1);    // [1]
    // writeConstant(&chunk, 2, 1);    // [1] [2]
    // writeConstant(&chunk, 3, 1);    // [1] [2] [3]
    // writeChunk(&chunk, OP_MULTIPLY, 1); // [1] [2*3]
    // writeChunk(&chunk, OP_ADD, 1);  // [1 + 2*3]
    // writeConstant(&chunk, 4, 1);    // [1 + 2*3] [4]
    // writeChunk(&chunk, OP_NEGATE, 1);   // [1 + 2*3] [-4]
    // writeConstant(&chunk, 5, 1);        // [1 + 2*3] [-4] [5]
    // writeChunk(&chunk, OP_NEGATE, 1);   // [1 + 2*3] [-4] [-5]
    // writeChunk(&chunk, OP_DIVIDE, 1);      // [(1 + 2*3)] [(-4 / -5)]
    // writeChunk(&chunk, OP_ADD, 1);      // [(1 + 2*3) + (-4 / -5)]
    // writeChunk(&chunk, OP_RETURN, 1);
    // interpret(&chunk);
    // freeVM();
    // freeChunk(&chunk);
    freeVM();

    return EXIT_SUCCESS;
}
