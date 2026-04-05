#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#define COMPILER_LL_LEN_MAX 1024
#define ARITY_MAX 0x1000000

ObjFunction_t *compile(const char *source);

typedef struct {
    int index;
    bool isLocal;
} Upvalue_t;

#endif // CLOX_COMPILER_H