# AGENTS.md

This file gives coding agents the minimum context needed to work safely in this repository.

## Project Scope

- Project: `wall-c` — small Wake-on-LAN CLI in C99.
- Primary code: `src/*.c`, declarations in `src/wall.h`.
- Tests: `tests/test.c` and `tests/integration_test.sh`.
- Build systems: `Makefile` and `build.zig`.
- Formal-verification experiment: `proof-plan.md`.

## Ground Rules

- Keep changes tightly scoped.
- Prefer deletion and explicit data flow over new persistent state or parser layers.
- Do not reintroduce a configuration-file/target-database feature without an explicit design decision.
- Do not introduce new dependencies unless requested and justified.
- Preserve current CLI behavior unless the task explicitly changes it.
- Prefer small, reviewable changes over broad refactors.
- Keep comments useful to a human reader; do not comment obvious syntax.

## Platform Targets

Treat support targets as:

- macOS: Apple Silicon ARM64 and Intel x86_64;
- Linux: ARM64 and x86_64.

Guard and justify platform-specific behavior. Networking/process API changes must consider both Linux and macOS paths.

## Build & Test

From repository root:

```sh
make
make test
make test-integration
make test-all
make sanitize
```

Optional:

```sh
make release
make memcheck
zig build
zig build test
zig build sanitize
zig build release
```

## Coding Conventions

- C99: `-std=c99`.
- POSIX feature set: `-D_POSIX_C_SOURCE=200809L`.
- Warnings are strict: `-Wall -Werror`.
- Prefer clear explicit control flow and fixed-size data where practical.
- Avoid new heap ownership unless it materially simplifies or enables required behavior.
- Avoid process-spawning/execution APIs in `src/`.
- Do not add public header/API surface without need.

## Testing Expectations

- Validation or input changes require unit coverage.
- CLI/network behavior changes require integration coverage when practical.
- Changes to help/output must keep `README.md`, `man/wall-c.1`, and completions aligned.
- Memory-sensitive changes should keep sanitizer CI green.

## High-Risk Areas

Verify carefully after edits:

- packet formatting: `src/packet.c`;
- CLI/input handling: `src/main.c`, `src/cli.c`;
- validation/parsing: `src/validate.c`;
- networking/interface resolution: `src/net.c`;
- smart probing and platform-specific memory parsing: `src/probe.c`;
- send orchestration: `src/engine.c`.

## Formal Verification Discipline

The proof experiment must bind results to exact source bytes. Do not describe a handwritten Lean rewrite as proof of the C implementation. Keep source proof, CompCert preservation, and reproducible-build claims separate. Update `proof-plan.md` when source simplification materially changes the proof surface.

## TODO Tracking

Keep `TODO.md` as the canonical current task list. Do not use it as a permanent changelog.

## Pre-Completion Checklist

Before finishing a code change:

1. build successfully;
2. run relevant unit/integration/sanitizer checks;
3. confirm no unintended files changed;
4. consider Linux/macOS and ARM64/x86_64 implications;
5. synchronize user-facing docs/completions;
6. state residual risks or unvalidated platform gaps.
