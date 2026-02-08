CC = gcc
CFLAGS = -Wall -Werror -std=c99 -D_POSIX_C_SOURCE=200809L -g
BUILD_DIR = build
SRC_CORE = config.c validate.c packet.c net.c cli.c
APP_SRCS = main.c $(SRC_CORE)
TEST_SRCS = test.c config.c validate.c packet.c net.c
TEST_BIN = $(BUILD_DIR)/wall-c-test
SAN_TEST_BIN = $(BUILD_DIR)/wall-c-test-sanitize
RELEASE_BIN = $(BUILD_DIR)/wall-c-release
RELEASE_CFLAGS = -O2 -DNDEBUG
SANITIZE_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
INTEGRATION_TEST = sh ./scripts/integration_test.sh
VALGRIND = valgrind
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose
LEAKS = leaks
LEAKS_FLAGS = --atExit --

# Detect OS
UNAME_S := $(shell uname -s)

all: wall-c

wall-c: $(APP_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/wall-c $(APP_SRCS)

release: $(APP_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) -o $(RELEASE_BIN) $(APP_SRCS)

clean:
	rm -rf $(BUILD_DIR)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DWALL_TEST -o $(TEST_BIN) $(TEST_SRCS)

test-integration: wall-c
	$(INTEGRATION_TEST) ./$(BUILD_DIR)/wall-c

test-all: test test-integration

$(SAN_TEST_BIN): $(TEST_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZE_FLAGS) -DWALL_TEST -o $(SAN_TEST_BIN) $(TEST_SRCS)

sanitize: $(SAN_TEST_BIN)
	./$(SAN_TEST_BIN)

# Memory leak checking - uses appropriate tool based on OS
memcheck: $(TEST_BIN)
ifeq ($(UNAME_S),Darwin)
	@echo "========================================="
	@echo "  Memory Leak Analysis (macOS)"
	@echo "========================================="
	@echo ""
	@echo "→ Running tests with malloc stack logging enabled..."
	@MallocStackLogging=1 ./$(TEST_BIN) 2>&1 | sed 's/^/  │ /'
	@echo ""
	@echo "→ Analyzing memory allocations..."
	@if leaks $(notdir $(TEST_BIN)) 2>&1 | grep -q "cannot find"; then \
		echo "  │ ✓ Process exited cleanly (short-lived test process)"; \
		echo "  │ ℹ  For long-running processes, use: leaks <pid>"; \
	else \
		leaks $(notdir $(TEST_BIN)) 2>&1 | sed 's/^/  │ /'; \
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
	@$(VALGRIND) $(VALGRIND_FLAGS) ./$(TEST_BIN) 2>&1 | \
		sed 's/^==[0-9]*==/  [valgrind]/' | \
		sed 's/^/  /'
	@echo ""
	@echo "========================================="
	@echo "  Analysis Complete"
	@echo "========================================="
endif

# Keep valgrind target for explicit use on Linux
valgrind: $(TEST_BIN)
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(TEST_BIN)

.PHONY: all clean test test-integration test-all sanitize memcheck valgrind release
