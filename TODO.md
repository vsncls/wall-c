## TODO (`wall-c`)

### Completed
- [x] Apply extensive inline comments and documentation in `wall.h`, `main.c`, and `test.c`.
- [x] Fix test build portability issue: replaced `mkdtemp` dependency in tests with portable temp-dir creation via `mkstemp` + `mkdir`.
- [x] Replace `sprintf` with `snprintf` in config path building (`main.c`).
- [x] Harden MAC parsing (`parse_mac`) by checking parse success for each byte.
- [x] Add strict error handling for test setup file/dir operations (`test.c`).
- [x] Define non-interactive behavior: require `-y` when stdin is not a TTY.
- [x] Add tests for non-interactive confirmation policy helper.
- [x] Extend MAC input support (lowercase, `-` separators, compact format) and normalize to canonical output.
- [x] Add tests for accepted/rejected MAC normalization and parser behavior.
- [x] Add GitHub Actions CI for `ubuntu-latest` and `macos-latest`.
- [x] Build/test with both `gcc` and `clang`.
- [x] Add sanitizer target (`-fsanitize=address,undefined`) and run it in CI.
- [x] Add unit tests for `build_magic_packet` byte layout.
- [x] Add CLI integration tests for exit codes and invalid argument combinations.

### Active Blocker
- [x] No active blocker.

### P0: Reliability (Do first)
- [x] Replace `sprintf` with `snprintf` in config path building (`main.c`).
- [x] Harden MAC parsing (`parse_mac`) by checking parse success for each byte.
- [x] Add strict error handling for test setup file/dir operations (`test.c`).
- [x] Define non-interactive behavior:
- [x] If stdin is not a TTY, require `-y` with clear error.
- [x] Add tests for chosen non-interactive behavior.
- [x] Extend MAC input support (lowercase, `-` separators, optional compact format) and normalize output.
- [x] Add tests for accepted and rejected MAC formats.

### P1: Testing + CI
- [x] Add GitHub Actions CI for `ubuntu-latest` and `macos-latest`.
- [x] Build/test with both `gcc` and `clang`.
- [x] Add sanitizer target (`-fsanitize=address,undefined`) and run it in CI.
- [x] Add unit tests for `build_magic_packet` byte layout.
- [x] Add CLI integration tests for exit codes and invalid argument combinations.

### P2: Maintainability
- [ ] Refactor `main.c` into modules: `config.c`, `validate.c`, `packet.c`, `net.c`, `cli.c`.
- [ ] Keep `main.c` as orchestration only.
- [ ] Align `Makefile` and `build.zig` targets (`test`, `release`, `sanitizer`).
- [ ] Update `README.md`:
- [ ] Document exit codes.
- [ ] Document non-interactive/script usage.
- [ ] Add automation examples.
- [ ] Add a `LICENSE` file.

### Done Criteria
- [ ] CI green on macOS + Linux.
- [ ] Sanitizer job has zero findings.
- [ ] `make test` passes locally and in CI.
- [ ] CLI behavior and docs are consistent.
