CC = clang
CXX = clang++

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TARGET = $(BIN_DIR)/program
TEST_IMG_PATH = assets/mavic_in_bush.jpg

CFLAGS_W = -Wall -Wno-unused-variable -Wno-unused-function -Wno-pointer-arith
CFLAGS_I = -Iinclude
CFLAGS = $(CFLAGS_W) $(CFLAGS_I) -O2

SIMD ?= 1
ifeq ($(SIMD),1)
    CFLAGS_SIMD = -DUSE_SIMD
    ARCH := $(shell uname -m)
    ifeq ($(ARCH),x86_64)
        CFLAGS_SIMD += -msse4.1 -mavx2
    else ifeq ($(ARCH),arm64)
        CFLAGS_SIMD += -mcpu=apple-m1
    else ifeq ($(ARCH),aarch64)
        CFLAGS_SIMD += -march=armv8-a+simd
    endif
endif

CFLAGS += $(CFLAGS_SIMD)

# platform dependent
LDFLAGS  = -lm -lpthread -ldl

SRCS = $(shell find $(SRC_DIR) -name "*.c")
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
OBJ_SUBDIRS = $(sort $(dir $(OBJS)))

.DEFAULT_GOAL := all

STB_HEADERS = include/foreign/stb_image.h include/foreign/stb_image_write.h

$(STB_HEADERS):
	@echo my dir $(@D)
	@mkdir -p $(@D)
	curl -sLo $@ https://raw.githubusercontent.com/nothings/stb/master/$(@F)

all: $(BIN_DIR) $(OBJ_SUBDIRS) $(STB_HEADERS) $(TARGET)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_SUBDIRS): $(OBJ_DIR)
	@mkdir -p $(OBJ_SUBDIRS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(STB_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(BIN_DIR) $(OBJ_SUBDIRS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

compile: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) $(TEST_IMG_PATH) -d 5

clean:
	rm -f $(OBJS)
	rm -rf $(BIN_DIR)/*

clean_output:
	rm -rf output/*

.PHONY: all prepare clean run

help:
	@echo "Available targets:"
	@echo "  all			: Compile the project"
	@echo "  run			: Run here"
	@echo "  clean			: Remove object files and executable"
