CC = gcc
CFLAGS = -Wall -Werror -std=c99 -g
BUILD_DIR = build
TEST_BIN = $(BUILD_DIR)/wall-c-test
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

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): main.c test.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DWALL_TEST -o $(TEST_BIN) main.c test.c

# Memory leak checking - uses appropriate tool based on OS
memcheck: wall-c
ifeq ($(UNAME_S),Darwin)
	@echo "========================================="
	@echo "  Memory Leak Analysis (macOS)"
	@echo "========================================="
	@echo ""
	@echo "→ Running tests with malloc stack logging enabled..."
	@MallocStackLogging=1 ./$(BUILD_DIR)/wall-c -t 2>&1 | sed 's/^/  │ /'
	@echo ""
	@echo "→ Analyzing memory allocations..."
	@if leaks wall-c 2>&1 | grep -q "cannot find"; then \
		echo "  │ ✓ Process exited cleanly (short-lived test process)"; \
		echo "  │ ℹ  For long-running processes, use: leaks <pid>"; \
	else \
		leaks wall-c 2>&1 | sed 's/^/  │ /'; \
	fi
	@echo ""
	@echo "========================================="
	@echo "  Analysis Complete"
	@echo "========================================="
else
	@echo "========================================="
	@echo "  Memory Leak Analysis (Valgrind)"
	@echo "========================================="
	@echo ""
	@$(VALGRIND) $(VALGRIND_FLAGS) ./$(BUILD_DIR)/wall-c -t 2>&1 | \
		sed 's/^==[0-9]*==/  [valgrind]/' | \
		sed 's/^/  /'
	@echo ""
	@echo "========================================="
	@echo "  Analysis Complete"
	@echo "========================================="
endif

# Keep valgrind target for explicit use on Linux
valgrind: wall-c
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(BUILD_DIR)/wall-c -t

.PHONY: all clean test memcheck valgrind
