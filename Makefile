CC = gcc
CXX = clang++

CFLAGS_W = -Wall -Wno-unused-variable -Wno-unused-function -Wno-pointer-arith
CFLAGS_I = -Iinclude
CFLAGS = $(CFLAGS_W) $(CFLAGS_I) -O2

# platform dependent
LDFLAGS  = $(RAYLIB_LINK) -lm -lpthread -ldl

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TARGET = $(BIN_DIR)/program

SRCS = $(wildcard $(SRC_DIR)/*.c) 
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: prepare $(TARGET)

prepare:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ_DIR)/*.o
	rm -rf $(BIN_DIR)/*

.PHONY: all prepare clean run

help:
	@echo "Available targets:"
	@echo "  all			: Compile the project"
	@echo "  run			: Run here"
	@echo "  clean			: Remove object files and executable"
