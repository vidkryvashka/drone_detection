CC = clang
CXX = clang++

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TARGET = $(BIN_DIR)/program
TEST_IMG_PATH = trash/old_images/raw/mavic_in_bush.png
DEFAULT_OUTPUT_DIR = output

CFLAGS_W = -Wall -Wno-unused-variable -Wno-unused-function -Wno-pointer-arith
CFLAGS_I = -Iinclude
CFLAGS = $(CFLAGS_W) $(CFLAGS_I) -DDEFAULT_OUTPUT_DIR=\"$(DEFAULT_OUTPUT_DIR)\" -O2

# platform dependent
LDFLAGS  = -lm -lpthread -ldl

SRCS = $(shell find $(SRC_DIR) -name "*.c")
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
OBJ_SUBDIRS = $(sort $(dir $(OBJS)))

all: prepare $(TARGET)

prepare:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_SUBDIRS)
	@mkdir -p $(DEFAULT_OUTPUT_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

compile: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) $(TEST_IMG_PATH)

clean:
	rm -f $(OBJS)
	rm -rf $(BIN_DIR)/*

clean_output:
	rm output/*

.PHONY: all prepare clean run

help:
	@echo "Available targets:"
	@echo "  all			: Compile the project"
	@echo "  run			: Run here"
	@echo "  clean			: Remove object files and executable"
