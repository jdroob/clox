#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, char *argv[]) {
    // for (int i=0; i<argc; ++i) {
    //     printf("arg%d: %s\n", i, argv[i]);
    // }
    
    Chunk_t chunk = {0};
    initChunk(&chunk);
    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);   // 'constant' is the offset in the val array where 1.2 can be found
    #ifdef DEBUG
        disassembleChunk(&chunk, "test chunk");
    #endif
    freeChunk(&chunk);
    return EXIT_SUCCESS;
}
