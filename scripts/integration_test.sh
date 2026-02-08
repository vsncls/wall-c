#!/bin/sh
set -eu

BIN_PATH="${1:-./build/wall-c}"

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
        echo "FAIL: $description (expected $expected_exit, got $actual_exit)" >&2
        exit 1
    fi
    echo "PASS: $description"
}

run_case 0 "help exits successfully" "$BIN_PATH" -h
run_case 1 "missing MAC exits with failure" "$BIN_PATH"
run_case 1 "invalid MAC exits with failure" "$BIN_PATH" -m "GG:BB:CC:DD:EE:FF" -y
run_case 1 "invalid IP exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -b "999.1.1.1" -y
run_case 1 "invalid non-numeric port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "abc" -y
run_case 1 "out-of-range port exits with failure" "$BIN_PATH" -m "AA:BB:CC:DD:EE:FF" -p "70000" -y
run_case 1 "non-interactive stdin requires -y" \
    sh -c '"$1" -m "AA:BB:CC:DD:EE:FF" < /dev/null' _ "$BIN_PATH"
run_case 1 "compact MAC accepted then invalid IP still fails on IP" "$BIN_PATH" -m "aabbccddeeff" -b "300.1.1.1" -y

echo "All CLI integration tests passed."
