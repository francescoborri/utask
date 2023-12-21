CC = gcc
CFLAGS = -std=c11 -Wall -Og -ggdb $(CMACROS)
LDLIBS = -lc
CMACROS = -D_GNU_SOURCE

BIN = bin
SRC = src
OBJ = obj

TARGET = $(BIN)/main

all : $(TARGET)

$(TARGET) : $(OBJ)/main.o $(OBJ)/utask.o $(OBJ)/ulock.o $(OBJ)/ucond.o
	mkdir -p $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ)/main.o : $(SRC)/main.c $(SRC)/utask.h $(SRC)/ulock.h
	mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o : $(SRC)/%.c $(SRC)/%.h
	mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	$(RM) $(TARGET) $(wildcard $(OBJ)/*.o)

compiledb :
	compiledb -o .vscode/compile_commands.json make -B

.PHONY : all clean compiledb
