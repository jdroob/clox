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

    #ifdef JRMALLOC
    char *buffer = (char *)jrmalloc(fileSize + 1);
    #else
    char *buffer = (char *)malloc(fileSize + 1);
    #endif
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
    InterpResult_t result = interpret(source);
    #ifdef JRMALLOC
    jrfree(source);
    #else
    free(source);
    #endif

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
    
    freeVM();

    return EXIT_SUCCESS;
}
