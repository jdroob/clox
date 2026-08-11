CC = cc
MAKE = make
INCLUDES = include
SRC = src
OBJ = obj
BIN = bin
LIBS = -lm
ARGS =
CFLAGS = -Wno-sequence-point -Wno-unused-function
DEBUG_CFLAGS = -DDEBUG
DEBUG_JRMALLOC_CFLAGS = -DDEBUG_JRMALLOC
JRMALLOC_CFLAGS = -DJRMALLOC
EXE = lox

SOURCES = $(wildcard $(SRC)/*.c)
OBJECTS = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

.PHONY: default setup clean

default: CFLAGS += -Wall -Wconversion -g -O0
default: $(EXE)

release: CFLAGS += -O2 -Wno-incompatible-pointer-types -Wno-discarded-qualifiers -Wno-stringop-overread
release: $(EXE)

$(EXE): $(OBJECTS)
	$(CC) $(OBJECTS) -I$(INCLUDES) $(LIBS) -o $(BIN)/$(EXE)

$(OBJ)/%.o: $(SRC)/%.c | setup
	$(CC) -c $< -I$(INCLUDES) $(LIBS) $(CFLAGS) -o $@

setup:
	mkdir -p $(OBJ)
	mkdir -p $(BIN)

just_see_bytecode: CFLAGS+=-DDEBUG_CHUNK -DJUST_SEE_BYTECODE
just_see_bytecode: default

debug: CFLAGS+=$(DEBUG_CFLAGS)
debug: default

debug_scanner: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_SCANNER
debug_scanner: default

debug_chunk: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_CHUNK
debug_chunk: default
view_stack: debug_chunk

debug_gc: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_STRESS_GC -DDEBUG_LOG_GC
debug_gc: default

debug_stress_gc: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_STRESS_GC
debug_stress_gc: default

debug_log_gc: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_LOG_GC
debug_log_gc: default

debug_chunk_jrmalloc: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_CHUNK -DJRMALLOC
debug_chunk_jrmalloc: default

debug_jrmalloc: CFLAGS+=$(DEBUG_JRMALLOC_CFLAGS) -DJRMALLOC
debug_jrmalloc: jrmalloc
	$(CC) $(SRC)/jrmalloc.c -I$(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN)/jrm

jrmalloc: CFLAGS+=$(JRMALLOC_CFLAGS)
jrmalloc: default

clean:
	rm -rf $(OBJ) $(BIN)

run:
	@$(BIN)/$(EXE) $(ARGS)
