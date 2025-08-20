#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, char *argv[]) {
    // for (int i=0; i<argc; ++i) {
    //     printf("arg%d: %s\n", i, argv[i]);
    // }
    
    Chunk_t chunk = {0};
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN, 123);
    writeConstant(&chunk, 1.2, 123);
    writeConstant(&chunk, 3.4, 456);
    writeConstant(&chunk, 5.6, 456);
    writeConstant(&chunk, 7.8, 456);
    writeConstant(&chunk, 9.10, 788);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    writeChunk(&chunk, OP_RETURN, 789);
    #ifdef DEBUG
        disassembleChunk(&chunk, "test chunk");
    #endif
    freeChunk(&chunk);
    return EXIT_SUCCESS;
}
