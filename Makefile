UTIL_PATH ?= ./util
SRC_PATH ?= ./src
CPPFLAGS += -I. -I$(SRC_PATH)
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -g -Wall
OBJ_PARSER = $(UTIL_PATH)/parser/parser.tab.o $(UTIL_PATH)/parser/parser.yy.o
OBJ = $(SRC_PATH)/main.o $(SRC_PATH)/cmd.o $(SRC_PATH)/utils.o
TARGET = custom-shell
.PHONY = all build clean build_parser

all: $(TARGET)

$(TARGET): build_parser $(OBJ) $(OBJ_PARSER)
	$(CC) $(CFLAGS) $(OBJ) $(OBJ_PARSER) -o $(TARGET)

$(SRC_PATH)/%.o: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

build_parser:
	$(MAKE) -C $(UTIL_PATH)/parser/

clean:
	-rm -rf $(OBJ) $(OBJ_PARSER) $(TARGET) *~
