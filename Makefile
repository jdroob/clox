CC = gcc
MAKE = make
INCLUDES = include
SRC = src
OBJ = obj
BIN = bin
ARGS =
CFLAGS = -Wall -g -Wno-sequence-point
DEBUG_CFLAGS = -DDEBUG
DEBUG_JRMALLOC_CFLAGS = -DDEBUG_JRMALLOC
JRMALLOC_CFLAGS = -DJRMALLOC
EXE = lox

SOURCES = $(wildcard $(SRC)/*.c)
OBJECTS = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

.PHONY: default setup clean

default: $(EXE)

$(EXE): $(OBJECTS)
	$(CC) $(OBJECTS) -I$(INCLUDES) -o $(BIN)/$(EXE)

$(OBJ)/%.o: $(SRC)/%.c | setup
	$(CC) -c $< -I$(INCLUDES) $(CFLAGS) -o $@

setup:
	mkdir -p $(OBJ)
	mkdir -p $(BIN)

debug: CFLAGS+=$(DEBUG_CFLAGS)
debug: default

debug_scanner: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_SCANNER
debug_scanner: default

debug_chunk: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_CHUNK
debug_chunk: default

debug_chunk_jrmalloc: CFLAGS +=$(DEBUG_CFLAGS) -DDEBUG_CHUNK -DJRMALLOC
debug_chunk_jrmalloc: default

debug_jrmalloc: CFLAGS+=$(DEBUG_JRMALLOC_CFLAGS) -DJRMALLOC
debug_jrmalloc: jrmalloc
	$(CC) $(SRC)/jrmalloc.c -I$(INCLUDES) $(CFLAGS) -o $(BIN)/jrm

jrmalloc: CFLAGS+=$(JRMALLOC_CFLAGS)
jrmalloc: default

clean:
	rm -rf $(OBJ) $(BIN)

run:
	@$(BIN)/$(EXE) $(ARGS)
