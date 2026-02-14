# AGENTS.md

This file gives coding agents the minimum context needed to work safely and quickly in this repository.

## Project Scope
- Project: `wall-c` (Wake-on-LAN CLI in C99)
- Primary code: `src/*.c`, shared declarations in `src/wall.h`
- Tests:
  - Unit tests: `tests/test.c`
  - CLI integration script: `tests/integration_test.sh`
- Build systems:
  - Make: `Makefile`
  - Zig build wrapper: `build.zig`

## Ground Rules
- Keep changes tightly scoped to the user request.
- Add and maintain very thorough comments aimed at beginner human coders when writing or changing code.
- Do not introduce new dependencies unless explicitly requested.
- Preserve current CLI behavior and flag compatibility unless asked to change it.
- Prefer small, reviewable patches over broad refactors.

## Platform Targets
- Treat support targets as:
  - macOS (Apple Silicon ARM64 + Intel x86_64)
  - Linux (ARM64 + x86_64)
- Avoid changes that are platform-specific unless guarded and justified.
- For behavior changes in networking/process APIs, verify assumptions on both macOS and Linux code paths.

## Build & Test
Use these commands from repo root:

```bash
make
make test
make test-integration
make test-all
make sanitize
```

Optional checks:

```bash
make release
make memcheck
```

Zig equivalents:

```bash
zig build
zig build test
zig build sanitize
zig build release
```

## Coding Conventions
- Language standard: C99 (`-std=c99`), POSIX APIs (`-D_POSIX_C_SOURCE=200809L`).
- Compiler warnings are strict (`-Wall -Werror`): keep code warning-free.
- Match existing style in touched files:
  - Keep includes and helpers local to where used.
  - Prefer clear, explicit control flow.
  - Avoid single-letter names except short loop indices.
- Do not add header/API surface unless needed for the task.

## Testing Expectations For Changes
- Behavior changes in parsing, CLI, networking, or validation should include test updates.
- For CLI-facing changes, update integration coverage in `tests/integration_test.sh` when applicable.
- If command output/help text changes, ensure tests and docs (`README.md`, `man/wall-c.1`, completions) stay aligned.

## High-Risk Areas
Treat these as sensitive and verify with tests after edits:
- Packet formatting and send path: `src/packet.c`, `src/net.c`
- CLI parsing and defaults: `src/cli.c`
- Config parsing and target selection: `src/config.c`, `src/engine.c`
- Validation logic: `src/validate.c`

## Documentation Sync
When user-visible behavior changes, update the relevant docs in the same change set:
- `README.md`
- `man/wall-c.1`
- Shell completions in `completions/`

## TODO Tracking
- Keep `TODO.md` as the canonical task list for repository work.
- When starting meaningful implementation work, add or update an item in `TODO.md`.
- Mark completed items clearly and avoid deleting historical context unless the user requests cleanup.
- If a task spans multiple commits, keep progress notes short and current in `TODO.md`.

## Pre-Completion Checklist
Before finishing a task, agents should:
1. Build successfully.
2. Run relevant tests for the touched area (at minimum `make test`; add integration/sanitize as needed).
3. Confirm no unintended file changes.
4. Confirm the change remains compatible with macOS/Linux on ARM64 and x86_64 (or document any gap).
5. Summarize what changed, why, and any residual risks.
