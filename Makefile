CC = gcc
MAKE = make
INCLUDES = include
SRC = src
OBJ = obj
BIN = bin
CFLAGS = -Wall -g
DEBUG_CFLAGS = -DDEBUG
DEBUG_JRMALLOC_CFLAGS = -DDEBUG_JRMALLOC
EXE = lox

# Get all source files and generate corresponding object file paths
SOURCES = $(wildcard $(SRC)/*.c)
OBJECTS = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

# Declare phony targets
.PHONY: default setup clean

# Default target
default: $(EXE)

# Link object files to create the executable
$(EXE): $(OBJECTS)
	$(CC) $(OBJECTS) -I$(INCLUDES) -o $(BIN)/$(EXE)

# Pattern rule to compile .c files to .o files
$(OBJ)/%.o: $(SRC)/%.c | setup
	$(CC) -c $< -I$(INCLUDES) $(CFLAGS) -o $@

# Create the object directory if it doesn't exist
setup:
	mkdir -p $(OBJ)
	mkdir -p $(BIN)

debug: CFLAGS+=$(DEBUG_CFLAGS)
debug: default

debug_jrmalloc: CFLAGS+=$(DEBUG_JRMALLOC_CFLAGS)
debug_jrmalloc: setup
	$(CC) $(SRC)/jrmalloc.c -I$(INCLUDES) $(CFLAGS) -o $(BIN)/jrm

# Clean up generated files
clean:
	rm -rf $(OBJ) $(BIN)

run:
	@$(BIN)/$(EXE) $(ARGS)
