#include "common.h"
#include "memory.h"
#include "table.h"
#include "compiler.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#endif


void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;
    #ifdef DEBUG_STRESS_GC
    if (newSize > oldSize) {
        collectGarbage();
    }
    #endif

    if (vm.bytesAllocated > vm.nextGC) {
        collectGarbage();
    }

    if (!newSize) {
        #ifdef JRMALLOC
        jrfree(pointer);
        #else
        free(pointer);
        #endif
        return NULL;
    }

    #ifdef JRMALLOC
    void *result = jrrealloc(pointer, newSize);
    #else
    void *result = realloc(pointer, newSize);
    #endif
    if (!result) {
        perror("Unable to grow array.");
        exit(EXIT_FAILURE);
    }
    return result;
}

static void freeObject(Obj_t *object) {
    #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->type);
    #endif
    switch (object->type) {
        case OBJ_CLASS: {
            // ObjClass_t *klass = (ObjClass_t *)object;
            // GC and freeObjects take care of klass->name
            FREE(ObjString_t, object);
            break;
        }
        case OBJ_STRING: {
            ObjString_t *string = (ObjString_t *)object;
            FREE(ObjString_t, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction_t *function = (ObjFunction_t *)object;
            // freeObject(function->name);  // <- GARBAGE COLLECTOR SHOULD TAKE CARE OF THIS LATER
            freeChunk(&function->chunk);
            FREE(ObjFunction_t, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative_t, object);
            break;
        }
        case OBJ_FILEHANDLE: {
            ObjFileHandle_t *objFH = (ObjFileHandle_t *)object; 
            FILE *fh = objFH->fh;
            if (IS_FILEHANDLE_OPEN(OBJ_VAL(objFH))) {
                fclose(objFH->fh);
                CLOSE_FILEHANDLE(OBJ_VAL(objFH));
            }
            FREE(ObjFileHandle_t, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure_t *closure = (ObjClosure_t *)object;
            FREE_ARRAY(ObjUpvalue_t*, closure->upvalues, closure->upvalueCount);
            // freeObject(closure->function);   // don't free function b/c multiple closures may enclose function
            FREE(ObjClosure_t, object);
            break;
        }
        case OBJ_UPVALUE: {
            // ObjUpvalue_t *upvalue = (ObjUpvalue_t *)object;
            FREE(ObjUpvalue_t, object);
            break;
        }
    }
}

void markObject(Obj_t *object) {
    if (object == NULL) return;
    if (object->isMarked == true) return;   // avoid cycles
    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void *)object);
        printValue(OBJ_VAL(object));
        printf("\n");
    #endif
    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj_t **)realloc(vm.grayStack, vm.grayCapacity * sizeof(Obj_t*));
    }
    if (!vm.grayStack) {
        fprintf(stderr, "memory.c::marObject::realloc");
        exit(EXIT_FAILURE);
    }
    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value_t value) {
    // If it's not an object, there's nothing to free
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

void markArray(ValueArray_t *array) {
    for (int i=0; i<array->count; ++i) {
        markValue(array->values[i]);
    }
}

static void markRoots(void) {
    for (Value_t *slot = vm.stack; slot < vm.stackTop; ++slot) {
        markValue(*slot);
    }
    for (size_t i=0; i<vm.frameCount; ++i) {
        markObject((Obj_t *)vm.frames[i].closure);
    }
    for (ObjUpvalue_t *upvalue = vm.openUpvalues;
        upvalue != NULL;
        upvalue = upvalue->next) {
            markObject((Obj_t *)upvalue);
    }
    markTable(&vm.globalNames);
    markArray(&vm.globalValues);
    markCompilerRoots();
}

static void blackenObject(Obj_t *ref) {
    #ifdef DEBUG_LOG_GC
        printf("%p blacken ", (void *)ref);
        printValue(OBJ_VAL(ref));
        printf("\n");
    #endif

    switch (ref->type) {
        case OBJ_CLASS: {
            ObjClass_t *klass = (ObjClass_t *)ref;
            markObject(klass->name);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure_t *closure = (ObjClosure_t *)ref;
            markObject(closure->function);
            for (int i=0; i<closure->upvalueCount; ++i) {
                markObject(closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction_t *function = (ObjFunction_t *)ref;
            markObject(function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_STRING:
        case OBJ_NATIVE:
        case OBJ_FILEHANDLE: {
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue_t *upvalue = (ObjUpvalue_t *)ref;
            markValue(upvalue->closed);
            break;
        }
    }
}

static void traceRefs(void) {
    while (vm.grayCount) {
        /**
         * There is no black stack.
         * Object x is black iff isMarked == true && !(x in grayStack)
         */
        blackenObject(vm.grayStack[--vm.grayCount]);
    }
}

static void sweep(void) {
    Obj_t *object = vm.objects;
    Obj_t *prev = NULL;
    while (object) {
        if (object->isMarked || IS_PROTECTED(OBJ_VAL(object))) {
            object->isMarked = false;   // set for next round of GC
            prev = object;
            object = object->next;
        } else {
            Obj_t *unreached = object;
            object = object->next;
            if (prev) prev->next = object;
            else vm.objects = object;
            freeObject(unreached);
        }
    }
}

void collectGarbage(void) {
    if (!vm.isInitialized) return;
    #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
    #endif

    markRoots();
    traceRefs();
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf(" collected %zu bytes (%zu -> %zu) next GC at %zu\n",
            before - vm.bytesAllocated, before, vm.bytesAllocated,
            vm.nextGC);
    #endif
}

void freeObjects(void) {
    Obj_t *next;
    Obj_t *object = vm.objects;
    while (object) {
        next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}
