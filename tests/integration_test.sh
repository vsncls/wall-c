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
run_case 0 "version exits successfully" "$BIN_PATH" --version
run_case 1 "missing MAC exits with failure" "$BIN_PATH"
run_case 1 "invalid MAC exits with failure" "$BIN_PATH" -m "GG:BB:CC:DD:EE:FF" -y
run_case 1 "invalid MAC with long option exits with failure" "$BIN_PATH" --mac "GG:BB:CC:DD:EE:FF" -y
run_case 1 "invalid IP exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -b "999.1.1.1" -y
run_case 1 "invalid non-numeric port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "abc" -y
run_case 1 "out-of-range port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "70000" -y
run_case 1 "invalid count exits with failure" "$BIN_PATH" --mac "AA:BB:CC:DD:EE:FF" --count 0 -y
run_case 1 "invalid interval exits with failure" "$BIN_PATH" --mac "AA:BB:CC:DD:EE:FF" --interval-ms -1 -y
run_case 1 "non-interactive stdin requires -y" \
    sh -c '"$1" -m "AA:BB:CC:DD:EE:FF" < /dev/null' _ "$BIN_PATH"
run_case 1 "compact MAC accepted then invalid IP still fails on IP" "$BIN_PATH" -m "aabbccddeeff" -b "300.1.1.1" -y
run_case 1 "interface and broadcast conflict exits with failure" \
    "$BIN_PATH" --mac "AA:BB:CC:DD:EE:FF" --interface lo0 -b "192.168.1.255" -y
run_case 0 "dry-run succeeds for valid MAC" \
    "$BIN_PATH" --mac "AA:BB:CC:DD:EE:FF" --dry-run --count 2 --interval-ms 0 -y

mkdir -p "$XDG_CONFIG_HOME/wall-c"
printf "nas AA:BB:CC:DD:EE:FF 192.168.1.255 9\nAA:BB:CC:DD:EE:11\n" > "$XDG_CONFIG_HOME/wall-c/config"
run_case 0 "list-targets exits successfully" "$BIN_PATH" --list-targets
run_case 0 "named target dry-run succeeds" "$BIN_PATH" --target "nas" --dry-run -y
run_case 1 "missing named target exits with failure" "$BIN_PATH" --target "does-not-exist" -y
run_case 1 "stdin MAC takes precedence over config when stdin MAC is invalid" \
    sh -c 'printf "not-a-mac\n" | "$1" -y' _ "$BIN_PATH"

printf "AA:BB:CC:DD:EE:FF\nnot-a-mac\n" > "$XDG_CONFIG_HOME/wall-c/config"
run_case 1 "config list fails on invalid line in dry-run mode" "$BIN_PATH" --dry-run -y

echo "${C_GREEN}All CLI integration tests passed.${C_RESET}"
