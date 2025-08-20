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
    int constant2 = addConstant(&chunk, 3.4);
    int constant3 = addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_RETURN, 123);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);   // 'constant' is the offset in the val array where 1.2 can be found
    writeChunk(&chunk, OP_CONSTANT, 456);
    writeChunk(&chunk, constant2, 456);   // 'constant2' is the offset in the val array where 1.2 can be found
    writeChunk(&chunk, OP_CONSTANT, 456);
    writeChunk(&chunk, constant3, 456);   // 'constant3' is the offset in the val array where 1.2 can be found
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
