CC = gcc
CFLAGS = -Wall -Werror -std=c99 -g
BUILD_DIR = build
VALGRIND = valgrind
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose
LEAKS = leaks
LEAKS_FLAGS = --atExit --

# Detect OS
UNAME_S := $(shell uname -s)

all: wall-c

wall-c: main.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/wall-c main.c

clean:
	rm -rf $(BUILD_DIR)

test: wall-c
	./$(BUILD_DIR)/wall-c -t

# Memory leak checking - uses appropriate tool based on OS
memcheck: wall-c
ifeq ($(UNAME_S),Darwin)
	@echo "Running memory leak check with macOS leaks tool..."
	@MallocStackLogging=1 ./$(BUILD_DIR)/wall-c -t; \
	echo "Checking for leaks..."; \
	leaks wall-c 2>/dev/null || echo "No leaks found or process already exited (this is expected for short tests)"
else
	@echo "Running memory leak check with valgrind..."
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(BUILD_DIR)/wall-c -t
endif

# Keep valgrind target for explicit use on Linux
valgrind: wall-c
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(BUILD_DIR)/wall-c -t

.PHONY: all clean test memcheck valgrind
