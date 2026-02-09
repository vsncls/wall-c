#!/bin/sh
set -eu

BIN_PATH="${1:-./build/wall-c}"

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RESET="$(printf '\033[0m')"
    C_GREEN="$(printf '\033[32m')"
    C_RED="$(printf '\033[31m')"
    C_BLUE="$(printf '\033[34m')"
else
    C_RESET=""
    C_GREEN=""
    C_RED=""
    C_BLUE=""
fi

if [ ! -x "$BIN_PATH" ]; then
    echo "integration-test: binary not executable: $BIN_PATH" >&2
    exit 1
fi

TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/wall-c-int-test.XXXXXX")"
cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT INT TERM

export XDG_CONFIG_HOME="$TEST_ROOT/xdg"
mkdir -p "$XDG_CONFIG_HOME"

run_case() {
    expected_exit="$1"
    description="$2"
    shift 2

    set +e
    "$@"
    actual_exit=$?
    set -e

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        echo "${C_RED}FAIL:${C_RESET} $description (expected $expected_exit, got $actual_exit)" >&2
        exit 1
    fi
    echo "${C_GREEN}PASS:${C_RESET} $description"
}

echo "${C_BLUE}Running CLI integration tests...${C_RESET}"
run_case 0 "help exits successfully" "$BIN_PATH" -h
run_case 1 "missing MAC exits with failure" "$BIN_PATH"
run_case 1 "invalid MAC exits with failure" "$BIN_PATH" -m "GG:BB:CC:DD:EE:FF" -y
run_case 1 "invalid IP exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -b "999.1.1.1" -y
run_case 1 "invalid non-numeric port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "abc" -y
run_case 1 "out-of-range port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "70000" -y
run_case 1 "non-interactive stdin requires -y" \
    sh -c '"$1" -m "AA:BB:CC:DD:EE:FF" < /dev/null' _ "$BIN_PATH"
run_case 1 "compact MAC accepted then invalid IP still fails on IP" "$BIN_PATH" -m "aabbccddeeff" -b "300.1.1.1" -y

mkdir -p "$XDG_CONFIG_HOME/wall-c"
printf "AA:BB:CC:DD:EE:FF\n" > "$XDG_CONFIG_HOME/wall-c/config"
run_case 1 "stdin MAC takes precedence over config when stdin MAC is invalid" \
    sh -c 'printf "not-a-mac\n" | "$1" -y' _ "$BIN_PATH"

printf "AA:BB:CC:DD:EE:FF\nnot-a-mac\n" > "$XDG_CONFIG_HOME/wall-c/config"
run_case 1 "config list wakes multiple entries and fails on invalid line" "$BIN_PATH" -y

echo "${C_GREEN}All CLI integration tests passed.${C_RESET}"
