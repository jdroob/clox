#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

uint8_t BUFFER[MAX_BUFF_LEN] = {0};

int main(int argc, char *argv[]) {
    initVM();
    
    Chunk_t chunk = {0};
    initChunk(&chunk);
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
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return EXIT_SUCCESS;
}
