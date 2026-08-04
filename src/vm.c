#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "common.h"
#include "compiler.h"
#include "value.h"
#include "object.h"


VM_t vm;

/**
 * Native functions
 */
static void runtimeError(const char *format, ...);
static bool isValidOperation(int requiredMode, const char *providedMode) {
    if (requiredMode == NULL || providedMode == NULL) {
        runtimeError("Invalid access type provided OR invalid file operation performed");
        return false;
    }
    if (strchr(providedMode, requiredMode) != NULL ||
        strchr(providedMode, '+')) {
        return true;
    }
    return false;
}

static Value_t clockNative(int argCount, Value_t *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value_t clockRealNative(int argCount, Value_t *args) {
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) != 0) {
        runtimeError("clock_gettime failed.");
        return ERR_VAL;
    }
    return NUMBER_VAL((double)(tp.tv_sec + tp.tv_nsec / 1e9));
}

static Value_t fopenNative(int argCount, Value_t *args) {
    if (!IS_STRING(args[0])) {
       runtimeError("arg0: open requires string argument type");
       return ERR_VAL; 
    }
    if (!IS_STRING(args[1])) {
        runtimeError("arg1: open requires string argument type");
        return ERR_VAL;
    } 
    char *fname = AS_CSTRING(args[0]);
    char *accessType = AS_CSTRING(args[1]);
    FILE *fh = fopen(fname, accessType);
    if (!fh) {
        runtimeError("Unable to find file: %s", fname);
        return ERR_VAL;
    }
    ObjFileHandle_t *fhObj = newFileHandle(fh, (const char *)fname, (const char *)accessType);
    return OBJ_VAL(fhObj);
}

static Value_t freadNative(int argCount, Value_t *args) {
    if (!IS_FILEHANDLE(args[0])) {
        runtimeError("read requires file handle argument type");
        return ERR_VAL;
    }
    ObjFileHandle_t *fhObj = AS_FILEHANDLE(args[0]);
    if (!isValidOperation('r', fhObj->accessType)) {
        runtimeError("Trying to read in non-read mode");
        return ERR_VAL;
    }
    FILE *fh = fhObj->fh;
    fseek(fh, 0, SEEK_END);
    long length = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    char contents[length + 1];
    size_t bytesRead = fread(contents, 1, length, fh);
    if (bytesRead != length) {
        runtimeError(
            "Error during read: Expected to read %ld bytes but instead read %lu bytes", 
            length, bytesRead);
        return ERR_VAL;
    }
    contents[length] = '\0';
    ObjString_t *wrappedContents = makeString(contents, length);
    return OBJ_VAL(wrappedContents);
}

static Value_t fwriteNative(int argCount, Value_t *args) {
    if (!IS_STRING(args[0])) {
        runtimeError("arg0: write requires string argument type");
        return ERR_VAL;
    }
    if (!IS_FILEHANDLE(args[1])) {
        runtimeError("arg1: write requires file handle argument type");
        return ERR_VAL;
    }
    ObjFileHandle_t *fhObj = AS_FILEHANDLE(args[1]);
    if (!isValidOperation('w', fhObj->accessType)) {
        runtimeError("Trying to write in non-write mode");
        return ERR_VAL;
    }
    ObjString_t *toWrite = AS_STRING(args[0]);
    FILE *fh = fhObj->fh;
    size_t len = toWrite->length;
    size_t bytesWritten = fwrite(toWrite->chars, 1, len, fh);
    if (bytesWritten != len) {
        runtimeError("Error occurred while writing to file");
        return ERR_VAL;
    }
    return NUMBER_VAL((double)bytesWritten);
}

static Value_t fcloseNative(int argCount, Value_t *args) {
    // return 0 on success? ERR_VAL on failure?
    Value_t fhVal = args[0];
    if (!IS_FILEHANDLE(fhVal)) {
        runtimeError("close requires file handle argument type");
        return ERR_VAL;
    }
    FILE *fh = AS_FILEHANDLE(args[0])->fh;
    int retVal = fclose(fh);
    if (retVal) {
        runtimeError("Error closing file");
        return ERR_VAL;
    }
    // AS_FILEHANDLE(fhVal)->isOpen = false;
    CLOSE_FILEHANDLE(fhVal);
    return NUMBER_VAL(0);
}

static Value_t lenNative(int argCount, Value_t *args) {
    // TODO: Add support for data structures as they become available
    if (!IS_STRING(args[0])) {
        runtimeError("len requires string type argument");
        return ERR_VAL;
    }
    size_t len = strnlen(AS_CSTRING(args[0]), LONG_MAX);    // seems reasonable?
    return NUMBER_VAL((double)len);
}

static Value_t getlineNative(int argCount, Value_t *args) {
    if (!IS_FILEHANDLE(args[0])) {
        runtimeError("getline requires file handle type argument");
        return ERR_VAL;
    }
    char *line = NULL;
    size_t len = 0;
    FILE *fh = AS_FILEHANDLE(args[0])->fh;
    ssize_t bytesRead = getline(&line, &len, fh);
    if (bytesRead == -1) {
        runtimeError("Error occurred in getline");
        return ERR_VAL;
    }
    ObjString_t *lineObj = makeString(line, (int)len);
    return OBJ_VAL(lineObj);
}

static Value_t hasattrNative(int argCount, Value_t *args) {
    if (!IS_INSTANCE(args[0])) {
        runtimeError("arg0: hasattr requires instance type");
        return ERR_VAL;
    }
    if (!IS_STRING(args[1])) {
        runtimeError("arg1: hasattr requires string type");
        return ERR_VAL;
    }
    ObjInstance_t *instance = AS_INSTANCE(args[0]);
    ObjString_t *attr = AS_STRING(args[1]);
    Value_t val;
    bool found = tableGet(&instance->fields, OBJ_VAL(attr), &val);
    return BOOL_VAL(found);
}

static Value_t getattrNative(int argCount, Value_t *args) {
    if (!IS_INSTANCE(args[0])) {
        runtimeError("arg0: getattr requires instance type");
        return ERR_VAL;
    }
    if (!IS_STRING(args[1])) {
        runtimeError("arg1: getattr requires string type");
        return ERR_VAL;
    }
    ObjInstance_t *instance = AS_INSTANCE(args[0]);
    ObjString_t *attr = AS_STRING(args[1]);
    Value_t val;
    bool found = tableGet(&instance->fields, OBJ_VAL(attr), &val);
    return found ? val : NIL_VAL;
}

static Value_t setattrNative(int argCount, Value_t *args) {
    if (!IS_INSTANCE(args[0])) {
        runtimeError("arg0: getattr requires instance type");
        return ERR_VAL;
    }
    if (!IS_STRING(args[1])) {
        runtimeError("arg1: getattr requires string type");
        return ERR_VAL;
    }
    ObjInstance_t *instance = AS_INSTANCE(args[0]);
    ObjString_t *attr = AS_STRING(args[1]);
    tableSet(&instance->fields, OBJ_VAL(attr), args[2]);
    return args[2];
}

static Value_t asciiNative(int argCount, Value_t *args) {
    // TODO: Implement me :)
    return ERR_VAL;
}

static void resetStack(void) {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

void freeVM(void);
static void runtimeError(const char *format, ...) {
    CallFrame_t *frame = &vm.frames[vm.frameCount - 1];
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

//    size_t instructionOffset = frame->ip - frame->closure->function->chunk.code - 1;
//    int line = getLine(&frame->closure->function->chunk, instructionOffset);
//    fprintf(stderr, "[line %d] in script\n", line);
    for (int i=vm.frameCount - 1; i>=0; --i) {
        ObjFunction_t *function = vm.frames[i].closure->function;
        unsigned instruction = vm.frames[i].ip - function->chunk.code - 1;    // -1 since ip points to instr after current instr
        int line = getLine(&function->chunk, instruction);
        
        fprintf(stderr, "[line %d]: ", line);
        if (function->name == NULL) {
            fprintf(stderr, "<script>\n");
        } else {
            fprintf(stderr, "<fn: %s>\n", function->name->chars);
        }
    }
    resetStack();
    //freeVM(); // freeing here will result in double free in main
}

static void defineNative(const char *funcName, NativeFn_t function, int arity) {
    /**
     * Pushing then immediately popping for GC purposes
     */
    ObjString_t *objStrFuncName = makeString(funcName, (int)strlen(funcName));
    ObjNative_t *objNativeFunc = newNative(function, arity);
    push(OBJ_VAL(objStrFuncName));
    push(OBJ_VAL(objNativeFunc));
    // write function name to global names table
    tableSet(&vm.globalNames, vm.stack[0], NUMBER_VAL(vm.globalValues.count));
    // write function object to global values table (function name --> function object)
    writeValueArray(&vm.globalValues, vm.stack[1]);
    pop();
    pop();
    turnOffProtectMode((Obj_t *)objStrFuncName);
    turnOffProtectMode((Obj_t *)objNativeFunc);
}

void initIsFinalsArray(MutableTable_t *array) {
    array->capacity = 0;
    array->count = 0;
    array->isFinalFlags = NULL;
}

void freeIsFinalsArray(MutableTable_t *array) {
    array->capacity = 0;
    array->count = 0;
    #ifdef JRMALLOC
    jrfree(array->isFinalFlags);
    #else
    free(array->isFinalFlags);
    #endif
    array->isFinalFlags = NULL;
}

void writeIsFinalsArray(MutableTable_t *array, bool flag) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->isFinalFlags = GROW_ARRAY(bool, array->isFinalFlags, oldCapacity, array->capacity);
    }
    array->isFinalFlags[array->count++] = flag;
}

void writeIsFinalsArrayAt(MutableTable_t *array, bool flag, unsigned idx) {
    // PRECONDITION: idx < array->count
    //   This function should only ever be called to update an existing isFinals flag
    if (array->capacity <= idx) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->isFinalFlags = GROW_ARRAY(bool, array->isFinalFlags, oldCapacity, array->capacity);
    }
    array->isFinalFlags[idx] = flag;
}

bool isLocalFinal(MutableTable_t *array, unsigned idx) {
    return array->isFinalFlags[idx];
}

void popLocalIsFinalFlag(MutableTable_t *array) {
    if (array->count) array->count--;
}

void initBreakJumpArray(BreakJump_t *array) {
    array->count = 0;
    array->capacity = 0;
    array->breakJumps = NULL;
}

void writeBreakJumpArray(BreakJump_t *array, int breakJump) {
    if (array->capacity < array->count + 1) {
        size_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->breakJumps = GROW_ARRAY(int, array->breakJumps, oldCapacity, array->capacity);
    }
    array->breakJumps[array->count++] = breakJump;
}

void freeBreakJumpArray(BreakJump_t *array) {
    array->capacity = 0;
    array->count = 0;
    #ifdef JRMALLOC
    jrfree(array->breakJumps);
    #else
    free(array->breakJumps);
    #endif
    array->breakJumps = NULL;
}

void resetBreakJumpArray(BreakJump_t *array) {
    array->count = 0;
}

static Value_t peek(int distance) {
    return vm.stackTop[-1 - distance];
}

// static bool isFalsey(Value_t value) {
//     return (IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)));
// }

static bool isTruthy(Value_t value) {
    return (IS_NUMBER(value) ? value.as.num != 0 : IS_BOOL(value) ? AS_BOOL(value) : false);
}

static bool isFalsey(Value_t value) {
    switch (value.type) {
        case VAL_BOOL: return !AS_BOOL(value);
        case VAL_NUM: return AS_NUMBER(value) == 0;
        case VAL_NIL: return true;
        case VAL_OBJ: {
            switch(OBJ_TYPE(value)) {
                case OBJ_STRING:
                    return AS_STRING(value)->length == 0;
            }
        }
        default: return false;
    }
}

bool valuesEqual(Value_t a, Value_t b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUM:  return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_NIL:  return IS_NIL(b);
        case VAL_EMPTY: return true;
        case VAL_OBJ:  {
            switch (OBJ_TYPE(a))
            {
                case OBJ_STRING:
                    return AS_STRING(a)->length == AS_STRING(b)->length &&
                           AS_STRING(a)->hash == AS_STRING(b)->hash &&
                            !memcmp(AS_CSTRING(a), AS_CSTRING(b), AS_STRING(a)->length);
            }
        }
        default: return false;
    }
}

static void concatenate(void) {
    // TODO: eventually replace protectMode approach with push / pop
    ObjString_t *b = AS_STRING(pop()); turnOnProtectMode((Obj_t *)b);
    ObjString_t *a = AS_STRING(pop()); turnOnProtectMode((Obj_t *)a);

    size_t length = a->length + b->length;
    char chars[length + 1];
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString_t *string = makeString(chars, length);
    push(OBJ_VAL(string));
    turnOffProtectMode((Obj_t *)string);
    turnOffProtectMode((Obj_t *)b);
    turnOffProtectMode((Obj_t *)a);
}

static void concatenateNum(void) {
    Value_t b = pop();
    Value_t a = pop();

    ObjString_t *str;
    double num;
    bool bIsString;
    str = IS_STRING(b) ? (bIsString = true, num = AS_NUMBER(a), AS_STRING(b)) : \ 
        (bIsString = false, num = AS_NUMBER(b), AS_STRING(a));
    turnOnProtectMode((Obj_t *)str);

    size_t len = str->length;
    #include <math.h>
    bool hasDecimalPart = fmod(num, 1.0) != 0.0;
    int truncated = (int)num;

    // Calculate total length
    if (hasDecimalPart) {
        len += snprintf(NULL, 0, "%g", num) + 1;
    } else {
        len += snprintf(NULL, 0, "%d", truncated) + 1;
    }

    char result[len];
    if (hasDecimalPart) {  // do not chop off decimal part
        if (bIsString) {   // a is num, b is string
            snprintf(result, len, "%g%s", num, str->chars);
        } else {           // a is string, b is num
            snprintf(result, len, "%s%g", str->chars, num);
        }
    } else {
        if (bIsString) {
            snprintf(result, len, "%d%s", truncated, str->chars);
        } else {
            snprintf(result, len, "%s%d", str->chars, truncated);
        }
    }

    ObjString_t *concatenated = makeString(result, len - 1);
    push(OBJ_VAL(concatenated));
    turnOffProtectMode((Obj_t *)concatenated);
    turnOffProtectMode((Obj_t *)str);
}

void push(Value_t value) {
    if (vm.capacity < vm.stackTop - vm.stack + 1) {
        uint32_t oldCapacity = vm.capacity;
        off_t offset = vm.stackTop - vm.stack;
        vm.capacity = GROW_CAPACITY(vm.capacity);
        vm.stack = GROW_ARRAY(Value_t, vm.stack, oldCapacity, vm.capacity);
        vm.stackTop = vm.stack + offset;
    }
    *vm.stackTop++ = value;
}

static void writeStackAt(unsigned idx, Value_t value) {
    *(vm.stackTop - 1 - idx) = value;
}

static Value_t getStackAt(unsigned idx) {
    return *(vm.stackTop - 1 - idx);
}

Value_t pop(void) {
    if (vm.stackTop == vm.stack) {
        // warning("Attempting to pop from empty stack");  // TODO: Implement me
        return NIL_VAL;
    }
    return *(--vm.stackTop);
}

void initVM(void) {
    vm.isInitialized = false;
    #ifdef JRMALLOC
    init(); // init jrmalloc
    #endif
    // vm.topLevel = NULL;
    // vm.ip = 0;
    vm.capacity = STACK_MAX;
    vm.switchCounter = 0;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = INIT_NEXT_GC;
    #ifdef JRMALLOC
    vm.stack = jrmalloc(STACK_MAX * sizeof(Value_t));
    #else
    vm.stack = ALLOCATE(Value_t, STACK_MAX);
    #endif
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globalNames);
    initValueArray(&vm.globalValues);
    initIsFinalsArray(&vm.globalIsFinals);
    resetStack();
    vm.openUpvalues = NULL;

    vm.initString = NULL;  // in case garbage collection happens in below call to makeSring
    vm.initString = makeString("init", 4);
    
    // define native functions
    defineNative("clock", clockNative, 0);
    defineNative("wallclock", clockRealNative, 0);
    defineNative("open", fopenNative, 2);
    defineNative("close", fcloseNative, 1);
    defineNative("read", freadNative, 1);
    defineNative("write", fwriteNative, 2);
    defineNative("getline", getlineNative, 1);
    defineNative("len", lenNative, 1);
    defineNative("hasattr", hasattrNative, 2);
    defineNative("getattr", getattrNative, 2);
    defineNative("setattr", setattrNative, 3);
    vm.isInitialized = true;
}

void freeVM(void) {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globalNames);
    freeValueArray(&vm.globalValues);
    freeIsFinalsArray(&vm.globalIsFinals);
    FREE_ARRAY(Value_t, vm.stack, vm.capacity);
    vm.initString = NULL;
}

void updateObjList(Obj_t *obj) {
    if (!vm.objects) {  // empty object list
        vm.objects = obj;
        obj->next = NULL;
        return;
    }
    Obj_t *curr = vm.objects;
    while (curr->next) curr = curr->next;
    curr->next = obj;
    obj->next = NULL;
}

static bool call(ObjClosure_t *closure, unsigned argCount) {
    ObjFunction_t *function = closure->function;
    if (function->arity != argCount) {
        runtimeError("Expected %d args for %.*s but received %d.", 
            function->arity, function->name->length, function->name->chars, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame_t *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = function->chunk.code;
    // frame->function = function;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool wasError(Value_t value) {
    return value.type == VAL_ERR;
}

static bool callValue(Value_t callee, unsigned argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            // case OBJ_FUNCTION:
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod_t *bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-(int)argCount - 1] = bound->receiver;  // set 'this' to receiver
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjInstance_t *instance = newInstance(AS_CLASS(callee));
                // replace class object with instance object
                vm.stackTop[-(int)argCount - 1] = OBJ_VAL(instance);
                turnOffProtectMode((Obj_t *)instance);
                Value_t initMethod;
                if (tableGet(&instance->klass->methods, OBJ_VAL(vm.initString), &initMethod)) {
                    return call(AS_CLOSURE(initMethod), argCount);
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but received %lu.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
               return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                ObjNative_t *func = (ObjNative_t *)AS_OBJ(callee);
                if (func->arity != argCount) {
                    runtimeError("native function expected %d arguments but received %u", func->arity, argCount);
                    return false;
                }
                NativeFn_t native = AS_NATIVE(callee);
                Value_t result = native(argCount, vm.stackTop - argCount);  // call native function
                if (wasError(result)) {
                    return false;
                }
                vm.stackTop -= argCount + 1;    // reset stack pointer
                push(result);
                if (IS_STRING(result)) turnOffProtectMode(AS_OBJ(result));
                return true;
            }
            default:
               break; // Non-callable object type
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static bool invokeFromClass(ObjClass_t *klass, ObjString_t *name, unsigned argCount) {
    Value_t method;
    bool found = tableGet(&klass->methods, OBJ_VAL(name), &method);
    if (!found) return false;
    return callValue(method, argCount);
}

static bool invoke(ObjString_t *name, unsigned argCount) {
    Value_t receiver = peek(argCount);
    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance_t *instance = AS_INSTANCE(receiver);
    Value_t value;
    if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
}

static ObjUpvalue_t *captureUpvalue(Value_t *local) {
    ObjUpvalue_t *prevUpvalue = NULL;
    ObjUpvalue_t *upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue_t *createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    turnOffProtectMode((Obj_t *)createdUpvalue);
    return createdUpvalue;
}

static void closeUpvalues(Value_t *last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue_t *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static bool bindMethod(ObjClass_t *klass, ObjString_t *name) {
    Value_t method;
    if (!tableGet(&klass->methods, OBJ_VAL(name), &method)) {
        runtimeError("Undefined property %s.", name->chars);
        return false;
    }

    ObjBoundMethod_t *boundMethod = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop(); // instance
    push(OBJ_VAL(boundMethod));
    return true;
}

static bool resolveListIndex(ObjList_t **lis, int *outIdx, unsigned offset) {
    /**
     * Validate index expression
     * Retrieve ObjList_t and Index objects
     */
    if (!IS_NUMBER(peek(offset))) {
        runtimeError("Index expression must be Number type.");
        return false;
    }
    if (!IS_LIST(peek(offset + 1))) {
        runtimeError("Index operand must be list type.");
        return false;
    }
    double dblIdx = AS_NUMBER(peek(offset));
    *lis = AS_LIST(peek(offset + 1));
    if (dblIdx < INT_MIN || dblIdx > INT_MAX) {
        runtimeError("List index (%d) is too large.", dblIdx);
        return false;
    }
    if (dblIdx != floor(dblIdx)) {
        runtimeError("Cannot use fractional indexes.");
        return false;
    }
    int idx = (int)dblIdx;
    if (idx < 0) {
        idx += (int)(*lis)->array.count;
    }
    if (idx < 0 || idx >= (int)(*lis)->array.count) {
        runtimeError("Index %d is out of bounds for list of length %u.",
        (int)dblIdx, (*lis)->array.count);
        return false;
    }
    *outIdx = idx;
    return true;
}

static InterpResult_t run(void) {
    CallFrame_t *frame = &vm.frames[vm.frameCount - 1];
    register uint8_t *ip = frame->ip;
    #define READ_BYTE() (*ip++)
    // #define READ_BYTE() (*frame->ip++)
    #define READ_BYTES() \
    ({ \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        unsigned bytes = (byte2 << 16) | (byte1 << 8) | byte0;  \
        bytes;  \
    })
    #define READ_SHORT() \
    ({ \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        uint16_t bytes = (byte1 << 8) | byte0; \
        bytes; \
    })
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
    //#define READ_CONSTANT() (vm.topLevel->chunk.constants.values[READ_BYTE()])
    /**
     * NOTE: below is a "statement expression"
     *  syntax:
     *      { stmt0; stmt1; ...; stmtN; expession; }
     *  The net effect is an "expression" that returns a value and has 0 or more side-effects.
     * 
     * NOTE: statement expressions are supported in GCC and Clang but NOT all C compilers support them.
    */
    #define READ_CONSTANT_LONG() \
    ({ \
        uint8_t byte2 = READ_BYTE(); \
        uint8_t byte1 = READ_BYTE(); \
        uint8_t byte0 = READ_BYTE(); \
        frame->closure->function->chunk.constants.values[(byte2 << 16) | (byte1 << 8) | byte0]; \
    })
    #define READ_STRING() (AS_STRING(READ_CONSTANT()))
    #define READ_STRING_LONG() (AS_STRING(READ_CONSTANT_LONG()))
    #define BINARY_OP(resType, operandType, op) do { \
        Value_t b = pop(); \
        Value_t a = pop(); \
        push(resType##_VAL((AS_##operandType(a) op AS_##operandType(b)))); \
    } while(false)
    #define IS_FINAL(idx) (vm.globalIsFinals.isFinalFlags[idx])

    for (;;) {
        uint8_t instruction;
        #ifdef DEBUG
        appendNewline = false;
        for (Value_t *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ] ");
        }
        puts("\n");
        disassembleInstruction(&frame->closure->function->chunk, (unsigned)(ip - frame->closure->function->chunk.code));
        appendNewline = true;
        #endif
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                Value_t retVal = pop(); // grab return value
                closeUpvalues(frame->slots);
                vm.frameCount--;        // pop off frame
                if (vm.frameCount == 0) {
                    pop(); // pop off <script>
                    return INTERPRET_OK;
                }
                vm.stackTop = frame->slots; // reset stack to top of previous frame
                frame = &vm.frames[vm.frameCount - 1];
                push(retVal);   // push return value to top of stack
                ip = frame->ip;
                break;
            }
            case OP_LIST:
            case OP_LIST_LONG: {
                unsigned len;
                if (instruction == OP_LIST_LONG) {
                    len = (unsigned)READ_BYTES();
                } else {
                    len = (unsigned) READ_BYTE();
                }

                ObjList_t *lis = newList(len);
                for (int i=len-1; i>=0; --i) {
                    writeValueArrayAt(&lis->array, pop(), i);
                }
                push(OBJ_VAL(lis)); turnOffProtectMode((Obj_t *)lis);
                break;
            }
            case OP_INDEX: {
                ObjList_t *lis;
                int idx;
                if (!resolveListIndex(&lis, &idx, 0)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(); // index value
                pop(); // list
                push(lis->array.values[idx]);
                break;
            }
            case OP_SLICE: {
                // lis[<expr1> : <expr2> ]
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Slicing expressions require number type inputs.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_NUMBER(peek(1))) {
                    runtimeError("Slicing expressions require number type inputs.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_LIST(peek(2))) {
                    runtimeError("Can only slice list type objects.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value_t end = pop(); // expr2 
                Value_t start = pop(); // expr1
                Value_t lis = pop(); // list 
                ObjList_t *slice = newSlice(AS_LIST(lis), (unsigned)AS_NUMBER(start), (unsigned)AS_NUMBER(end));
                push(OBJ_VAL(slice)); turnOffProtectMode((Obj_t *)slice);
                break;
            }
            case OP_SLICE_UNTIL: {

            }
            case OP_SLICE_REST: {

            }
            case OP_SLICE_WHOLE: {

            }
            case OP_SET_INDEX: {
                ObjList_t *lis;
                int idx;
                if (!resolveListIndex(&lis, &idx, 1)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value_t val = pop(); // value to be assigned
                pop(); // index value
                pop(); // list
                writeValueArrayAt(&lis->array, val, (unsigned)idx);
                push(val);
                break;
            }
            case OP_CONSTANT: {
                Value_t constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value_t constant = READ_CONSTANT_LONG();
                push(constant);
                break;
            }
            case OP_CLASS: {
                ObjClass_t *klass = newClass(READ_STRING());
                push(OBJ_VAL(klass)); turnOffProtectMode((Obj_t *)klass);
                break;
            }
            case OP_CLASS_LONG: {
                ObjClass_t *klass = newClass(READ_STRING_LONG());
                push(OBJ_VAL(klass)); turnOffProtectMode((Obj_t *)klass);
                break;
            }
            case OP_CLOSURE: 
            case OP_CLOSURE_LONG: {
                ObjFunction_t *function = (instruction == OP_CLOSURE) ? 
                                          AS_FUNCTION(READ_CONSTANT()) :
                                          AS_FUNCTION(READ_CONSTANT_LONG());
                ObjClosure_t *closure = newClosure(function);
                push(OBJ_VAL(closure));
                /**
                 * fun outer() {
                 *    var x = 0;
                 *    fun inner() { <-- Imagine you're here (outer has been called - executing **function declaration** `inner`)
                 *       print x;
                 *    }
                 * }
                 */
                for (int i=0; i<closure->upvalueCount; ++i) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE(); // TODO: Need to allow for 3-byte indices as well
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);    // frame->slots points to beginning of outer's stack window
                    } else {  // nested closure referring to upvalue
                        closure->upvalues[i] = frame->closure->upvalues[index];         // point to ObjUpvalue_t object that enclosing function points to 
                    }
                }
                turnOffProtectMode((Obj_t *)closure);
                turnOffProtectMode((Obj_t *)function);
                break;
            }
            case OP_METHOD:
            case OP_METHOD_LONG: {
                ObjString_t *methodName;
                if (instruction == OP_METHOD) {
                    methodName = READ_STRING();
                } else {
                    methodName = READ_STRING_LONG();
                }
                // pop ObjClosure from stack
                ObjClosure_t *closure = AS_CLOSURE(pop());
                // add to class's methods table
                ObjClass_t *klass = AS_CLASS(peek(0));
                tableSet(&klass->methods, OBJ_VAL(methodName), OBJ_VAL(closure));
                break;
            }
            case OP_DEL_IDCTOR: {
                if (!IS_STRING(peek(0))) {
                    runtimeError("Constructed identifier must be string type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Left operand to '.' operator must be instance.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString_t *property = AS_STRING(pop());
                ObjInstance_t *instance = AS_INSTANCE(pop());
                bool found = tableDelete(&instance->fields, OBJ_VAL(property));
                // if (!found) {
                // TODO: implement me :)
                //     runtimeWarning("attribute %s not found.", property->chars);
                // }
                break;
            }
            case OP_GET_PROPERTY_IDCTOR: {
                if (!IS_STRING(peek(0))) {
                    runtimeError("Constructed identifier must be string type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Left operand to '.' operator must be instance.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString_t *property = AS_STRING(pop());
                ObjInstance_t *instance = AS_INSTANCE(peek(0));
                Value_t value;
                if (tableGet(&instance->fields, OBJ_VAL(property), &value)) {
                    pop();  // instance
                    push(value);
                    break;
                }
                if (!bindMethod(instance->klass, property)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY_IDCTOR: {
                if (!IS_STRING(peek(1))) {
                    runtimeError("Constructed identifier must be string type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_INSTANCE(peek(2))) {
                    runtimeError("Left operand to '.' operator must be instance.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value_t value = pop();
                ObjString_t *property = AS_STRING(pop());
                ObjInstance_t *instance = AS_INSTANCE(pop());
                tableSet(&instance->fields, OBJ_VAL(property), value);
                push(value);    // since this is an assignment expression
                break;
            }
            case OP_DEL:
            case OP_DEL_LONG: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("'del' operator expects instance attribute as operand.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString_t *property;
                if (instruction == OP_DEL){
                    property = READ_STRING();
                } else {
                    property = READ_STRING_LONG();
                }
                ObjInstance_t *instance = AS_INSTANCE(pop());
                bool found = tableDelete(&instance->fields, OBJ_VAL(property));
                // if (!found) {
                // TODO: implement me :)
                //     runtimeWarning("attribute %s not found.", property->chars);
                // }
                break;
            }
            case OP_INHERIT: {
                Value_t superclass = peek(1);  // second from top is superclass (OBJ_CLASS)
                if (!IS_CLASS(superclass)) {
                    runtimeError("Classes can only inherit from other classes.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass_t *subclass = AS_CLASS(peek(0));    // top is subclass (OBJ_CLASS)
                tableAddAll(&AS_CLASS(superclass)->methods, 
                            &subclass->methods);
                pop();  // pop subclass from top of stack; superclass remains at top so we can still reference via super
                break;
            }
            case OP_INVOKE:
            case OP_INVOKE_LONG: {
                ObjString_t *method;
                if (instruction == OP_INVOKE) {
                    method = READ_STRING();
                } else {
                    method = READ_STRING_LONG();
                }
                unsigned argCount = (unsigned)READ_BYTE();
                frame->ip = ip;
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1]; // pop method's frame
                ip = frame->ip;
                break;
            }
            case OP_SUPER_INVOKE:
            case OP_SUPER_INVOKE_LONG: {
                ObjString_t *methodName;
                if (instruction == OP_SUPER_INVOKE) {
                    methodName = READ_STRING();
                } else {
                    methodName = READ_STRING_LONG();
                }
                unsigned argCount = (unsigned)READ_BYTE();
                ObjClass_t *superklass = AS_CLASS(pop());  // pop superclass
                frame->ip = ip;
                if (!invokeFromClass(superklass, methodName, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1]; // pop method's frame
                ip = frame->ip;
                break;

            }
            case OP_GET_SUPER:
            case OP_GET_SUPER_LONG: {
                if (!IS_CLASS(peek(0))) {
                    runtimeError("Corrupt stack - top of stack should be superclass at OP_GET_SUPER execution.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass_t *superklass = AS_CLASS(pop());  // pop superclass

                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Corrupt stack - stackTop[1] should be instance of 'this' at OP_GET_SUPER execution.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                // ObjInstance_t *receiver = AS_INSTANCE(peek(1));

                ObjString_t *property;
                if (instruction == OP_GET_SUPER){
                    property = READ_STRING();
                } else {
                    property = READ_STRING_LONG();
                }

                if (!bindMethod(superklass, property)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_PROPERTY:
            case OP_GET_PROPERTY_LONG: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Left operand to '.' operator must be instance.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString_t *property;
                if (instruction == OP_GET_PROPERTY){
                    property = READ_STRING();
                } else {
                    property = READ_STRING_LONG();
                }
                ObjInstance_t *instance = AS_INSTANCE(peek(0));
                Value_t value;
                if (tableGet(&instance->fields, OBJ_VAL(property), &value)) {
                    pop();  // instance
                    push(value);
                    break;
                }
                if (!bindMethod(instance->klass, property)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY:
            case OP_SET_PROPERTY_LONG: {
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Left operand to '.' operator must be instance.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                // Read name of property from constant pool
                ObjString_t *property;
                if (instruction == OP_SET_PROPERTY) {
                    property = READ_STRING();
                } else {
                    property = READ_STRING_LONG();
                }
                // pop value to be assigned from top of stack
                Value_t value = pop();  // rhs
                // pop instance containing fields map
                ObjInstance_t *instance = AS_INSTANCE(pop());
                // set instance.property = value
                tableSet(&instance->fields, OBJ_VAL(property), value);
                push(value);    // since this is an assignment expression
                break;
            }
            case OP_ACCESS_UPVALUE:
            case OP_ACCESS_UPVALUE_LONG: {
                unsigned slot = (instruction == OP_ACCESS_UPVALUE) ? READ_BYTE() : READ_BYTES();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE:
            case OP_SET_UPVALUE_LONG: {
                unsigned idx = (instruction == OP_SET_UPVALUE) ? READ_BYTE() : READ_BYTES();
                *frame->closure->upvalues[idx]->location = peek(0);
                break;
            }
            // case OP_CLOSURE_LONG: {
            //     ObjFunction_t *function = AS_FUNCTION(READ_CONSTANT_LONG());
            //     ObjClosure_t *closure = newClosure(function);
            //     push(OBJ_VAL(closure));
            //     break;
            // }
            case OP_TRUE: {
                push(BOOL_VAL(true));
                break;
            }
            case OP_FALSE: {
                push(BOOL_VAL(false));
                break;
            }
            case OP_ZERO: {
                push(NUMBER_VAL(0));
                break;
            }
            case OP_ONE: {
                push(NUMBER_VAL(1));
                break;
            }
            case OP_NEG_ONE: {
                push(NUMBER_VAL(-1));
                break;
            }
            case OP_NIL: {
                push(NIL_VAL);
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    frame->ip = ip;
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (vm.stackTop != vm.stack) 
                    (vm.stackTop - 1)->as.num = -AS_NUMBER(*(vm.stackTop - 1));
                break;
            }
            case OP_NOT: {
                push(BOOL_VAL(isFalsey(pop())));
                break;
            }
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    BINARY_OP(NUMBER, NUMBER, +); 
                } else if (IS_NUMSTR(peek(0), peek(1))) {
                    frame->ip = ip;
                    concatenateNum();
                } else {
                    frame->ip = ip;
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER, NUMBER, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER, NUMBER, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER, NUMBER, /); break;
            case OP_GT:         BINARY_OP(BOOL, NUMBER, >); break;
            case OP_LT:         BINARY_OP(BOOL, NUMBER, <); break;
            case OP_MODULO: {
                Value_t b = pop();
                Value_t a = pop();
                double res = (double)((int)AS_NUMBER(a) % (int)AS_NUMBER(b));
                push(NUMBER_VAL(res));
                break;
            }
            case OP_EQ: {
                Value_t b = pop();
                Value_t a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_POP: {
                pop();
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OP_PRINT: {
                printValue(pop());
                break;
            }
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
                    frame->ip = ip;
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
                    frame->ip = ip;
                    runtimeError("Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_FINAL(idx)) {
                    frame->ip = ip;
                    runtimeError("Cannot assign to 'final' variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                writeValueArrayAt(&vm.globalValues, peek(0), idx);
                break;
            }
            case OP_ACCESS_LOCAL:
            case OP_ACCESS_LOCAL_LONG: {
                unsigned idx;
                if (instruction == OP_ACCESS_LOCAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }
                push(frame->slots[idx]);
                break;
            }
            case OP_SET_LOCAL:
            case OP_SET_LOCAL_LONG: {
                unsigned idx;
                if (instruction == OP_SET_LOCAL) {
                    idx = (unsigned)READ_BYTE();
                } else {
                    idx = READ_BYTES();
                }

                // writeStackAt(idx, peek(0));
                frame->slots[idx] = peek(0);
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_SHORT();
                if (!isFalsey(peek(0))) {
                    ip += offset;
                    break;
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) {
                    ip += offset;
                    break;
                }

                break;

                // Value_t value = peek(0);
                // if (falsey(value)) {
                //     uint16_t offset = 0;
                //     offset = (offset | *vm.ip) << 8;
                //     offset = offset | *(vm.ip + 1);
                //     vm.ip += offset;
                //     break;
                // }
                // vm.ip += 2; // Skip offset
                // break;
            }
            // There need not be 2 ops for this
            case OP_BREAK:
            case OP_BREAKALL: {
                unsigned popCount = READ_BYTES();
                while (popCount--) pop();
                break;
            }
            case OP_SWITCH: {
                vm.switchCounter++;   // account for switch expression
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            case OP_CASE: {
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                vm.switchCounter++; // to account for case expression
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            // case OP_DEFAULTCASE: {
            //     printf("vm.switchCounter: %d\n", vm.switchCounter);
            //     break;
            // }
            case OP_ENDSWITCH: {
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                if (vm.switchCounter < 0) frame->ip = ip, runtimeError("Stack in invalid state post-switch.");
                
                /**
                 * precondition: vm.switchCounter >= prevSwitchDepth
                 * 
                 * For each 'switch', vm.switchCounter was incremented
                 * Thus, when exiting a 'switch', we only want to decrement
                 * to the previous switch depth (not necessarily 0).
                 */
                uint8_t prevSwitchDepth = READ_BYTE();
                while (vm.switchCounter > prevSwitchDepth) {
                    pop();
                    vm.switchCounter--;
                }
                // printf("vm.switchCounter: %d\n", vm.switchCounter);
                break;
            }
            case OP_JUMP_IF_NOT_MATCH: {
                uint16_t offset = READ_SHORT();
                if (valuesEqual(peek(0), peek(1))) {
                    pop();  // pop case expression
                    pop();  // pop switch expression
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    vm.switchCounter -= 2;
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                } else {
                    pop();  // pop case expression
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    vm.switchCounter--;
                    // printf("vm.switchCounter: %d\n", vm.switchCounter);
                    ip += offset;  // jump to next case or default
                }
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                break;
            }
            case OP_CALL:
            case OP_CALL_LONG: {
                unsigned argCount;
                if (instruction == OP_CALL) {
                    argCount = (unsigned)READ_BYTE();
                } else {
                    argCount = READ_BYTES();
                }
                frame->ip = ip; // when caller resumes, frame->ip is correct
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            default:
        }
    }

    #undef READ_CONSTANT_LONG
    #undef READ_CONSTANT
    #undef READ_STRING_LONG
    #undef READ_STRING
    #undef READ_BYTE
    #undef READ_SHORT
    #undef IS_FINAL

    return INTERPRET_OK;
}

InterpResult_t interpret(const char *source) {
    ObjFunction_t *function = NULL;
    
    // We're getting the top-level function back
    if ((function = compile(source)) == NULL) {
        // freeChunk(&function->chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    ObjClosure_t *closure = newClosure(function);
    pop();  // pop off what we just pushed (GC reasons)..
    push(OBJ_VAL(closure));
    turnOffProtectMode((Obj_t *)closure);
    call(closure, 0);
    // CallFrame_t *frame = &vm.frames[vm.frameCount++];
    // frame->function = function;
    // frame->ip = function->chunk.code;
    // frame->slots = vm.stack;
    // vm.chunk = &chunk;
    // vm.function = function;
    // vm.ip = vm.topLevel->chunk.code;

    // InterpResult_t result = run();

    // freeChunk(&function->chunk);
    // return result;
    return run();
}
