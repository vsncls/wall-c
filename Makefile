CC = gcc
CFLAGS = -Wall -Werror -std=c99 -g
BUILD_DIR = build

all: wall-c

wall-c: main.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/wall-c main.c

clean:
	rm -rf $(BUILD_DIR)

test: wall-c
	./$(BUILD_DIR)/wall-c -t
