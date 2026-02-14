## TODO (`wall-c`)

### Completed (Recent)

- [x] Documentation quality:
- [x] Expanded beginner-focused explanatory comments across `src/*.c` without behavior changes.

- [x] CLI 2.0:
- [x] Added long flags (`--mac`, `--broadcast`, `--port`, `--yes`, `--help`, `--version`).
- [x] Added automation flags (`--dry-run`, `--quiet`).
- [x] Added batch controls (`--count`, `--interval-ms`, `--continue-on-error`).
- [x] Host targets:
- [x] Added named config target format (`NAME MAC [BROADCAST_IP] [PORT]`).
- [x] Added `--target <name>` and `--list-targets`.
- [x] Network usability:
- [x] Added `--interface <ifname>` broadcast resolution.
- [x] Packaging:
- [x] Added `make install` / `make uninstall`.
- [x] Added man page at `man/wall-c.1`.
- [x] Added zsh/fish completion delivery via install target.
- [x] Testing/CI:
- [x] Expanded unit + integration tests for new CLI/config behavior.
- [x] Added sanitizer CI coverage on Ubuntu + macOS.
- [x] Added shell lint job for `tests/integration_test.sh`.
- [x] Docs:
- [x] Updated `README.md` for new options, config format, install flow, and troubleshooting.

### Next Candidates

- [ ] Smart option with homegrown arping to a. avoid waking if already up and b. loop retry with a timeout while waiting for arpong
- [ ] Add `--timeout-ms` for send retry pacing in unstable environments.
- [ ] Add interface-selection completion hints for fish from live interfaces.

### Done Criteria

- [ ] CI green on macOS + Linux for ARM64 and x86_64 after merge.
- [ ] Sanitizer jobs remain zero findings.
- [ ] Release workflow validated on first `v*` tag.
