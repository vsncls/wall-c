# Lean 4 memory-safety proof plan

## Status

This is an engineering/research plan, not a claim that `wall-c` is already
formally verified.

The proof must bind itself to exact source bytes. The initial study target was
commit `c3d4c2322425a013e182ea79804bdc059a104b25`; once proof-driven C changes land,
the verified source revision and file hashes must be updated explicitly.

## Primary goal

Prove in Lean 4 that the actual `wall-c` C source is memory safe for all modeled
executions satisfying explicit C-library and POSIX contracts.

Conceptually:

```lean
theorem wall_c_memory_safe
    (input : ProcessInput)
    (s0 s1 : CState)
    (hinput : ValidProcessInput input)
    (hexec : Exec wallCProgram input s0 s1) :
    ¬ HasMemoryUB s1
```

`HasMemoryUB` should cover at least:

- out-of-bounds reads and writes;
- null/invalid pointer dereferences;
- use-after-free;
- double-free and invalid free;
- uninitialized reads where C makes them undefined;
- invalid overlap for operations such as `memcpy`;
- allocation-size arithmetic overflow leading to undersized objects;
- lifetime/provenance violations represented by the chosen model.

Heap leaks are a separate theorem:

```lean
theorem wall_c_no_owned_heap_leaks_on_normal_exit :
  NormalTermination e → AllWallOwnedAllocationsReleased e
```

## Proof levels

### Level 1 — Lean model

Reimplement relevant algorithms as Lean functions and prove them correct.
Useful for discovering invariants, but insufficient as the final claim because
the C/Lean correspondence would remain unproved.

### Level 2 — actual C source — primary target

Mechanically parse the actual C source into an AST, give the exercised C
fragment formal semantics in Lean, and prove memory safety of that AST.

A handwritten Lean transcription may assist development, but must not be the
sole bridge used by the final theorem.

### Level 3 — compiled artifact — active secondary target

Carry the source result toward assembly through CompCert, inside a hermetic Nix
build. This is now part of the plan rather than a vague future possibility, but
it remains a distinct theorem boundary.

The intended chain is:

```text
pinned wall-c source
        |
        | source hashes
        v
Lean C importer / AST
        |
        v
Lean C-fragment semantics
        |
        v
Lean memory-safety theorem
        |
        | semantic bridge to CompCert C semantics
        v
CompCert verified compiler core
        |
        v
assembly
        |
        | pinned assembler/linker/sysroot
        v
reproducible executable
```

The difficult research seam is not primarily compilation itself; it is the
relationship between the Lean semantics and CompCert's Rocq/Coq semantics.

## Determinism and reproducibility

Do not equate three different claims:

1. **source proof** — Lean proves the modeled C program memory safe;
2. **compiler correctness** — CompCert proves semantic preservation through its
   verified compiler passes;
3. **reproducible build** — identical pinned inputs produce identical output
   bits.

Nix addresses the third property, not the first two.

The repository therefore contains a draft `flake.nix` intended to pin:

- nixpkgs;
- CompCert;
- Lean 4;
- ordinary C compilers used for differential/sanitizer testing;
- build environment variables affecting reproducibility.

The generated `flake.lock` is the actual immutable input pin and must be
committed before reproducibility is claimed.

A future CI reproducibility check should build from the same lockfile in two
independent builders and compare output hashes.

## CompCert boundary

CompCert is attractive because its verified compiler core proves preservation
of source semantics through compilation to assembly for supported targets.

However, the full invocation pipeline still has external seams such as
preprocessing, parsing/front-end machinery, assembly and linking. These must be
listed explicitly rather than absorbed into a vague "verified compiler" claim.

The preferred verification pipeline is therefore:

```text
exact C source
   |
   +--> exact preprocessed source, if preprocessing is retained
   |       hash this artifact
   v
formal source semantics
   |
   +--> Lean theorem
   |
   +--> semantic correspondence with CompCert representation
   v
CompCert generated assembly
   |
   +--> hash
   v
pinned assembler + linker + sysroot
   |
   +--> hash
   v
executable
```

Whenever possible, reduce preprocessing complexity and compiler-specific
extensions in `wall-c` rather than broadening the formal front end.

## Why `wall-c` is a suitable target

The program is unusually tractable for this experiment:

- small C99 codebase;
- sequential control flow;
- no threads;
- explicit ownership cleanup helpers;
- fixed-size packet buffers;
- very limited pointer arithmetic;
- bounded packet construction;
- narrow POSIX/network boundary;
- existing unit, integration, ASan and UBSan tests.

The most interesting ownership work remains `src/config.c`, particularly list
growth, `realloc`, partial construction and cleanup.

## Lean proof architecture

```text
actual wall-c C source
        |
        v
mechanical parser / AST
        |
        v
formal C fragment semantics
        |
        +--> libc contracts
        +--> POSIX contracts
        |
        v
per-function proofs
        |
        v
ownership/module invariants
        |
        v
whole-program memory-safety theorem
```

Proposed layout:

```text
proof/
  WallC/
    Syntax.lean
    Source.lean
    Memory.lean
    Semantics.lean
    Hoare.lean

    LibC/
      Alloc.lean
      CString.lean
      MemoryOps.lean
      Stdio.lean
      Parse.lean

    Posix/
      Socket.lean
      Time.lean
      Environment.lean

    Wall/
      Validate.lean
      Packet.lean
      Config.lean
      Cli.lean
      Net.lean
      Probe.lean
      Engine.lean
      Main.lean

    Bridge/
      CompCert.lean
      SourceHashes.lean

    Theorems.lean
```

## C fragment to model

Do not formalize all of ISO C up front. Model only the fragment actually used:

- integer and character scalar types;
- arrays and structs;
- pointers/null;
- stack and heap objects;
- address-of/dereference;
- field and array access;
- pointer arithmetic used by the program;
- assignments;
- conditionals;
- loops;
- function calls/returns;
- casts used in source;
- `sizeof`;
- relevant integer-conversion behavior;
- allocation failure paths.

Unsupported syntax must make source import fail loudly.

## Memory model

At minimum the model must represent live allocations, sizes, writeability and
initialization state. Pointer provenance may require a richer representation.

Useful predicates include:

```lean
Readable   : Memory → Ptr → Nat → Prop
Writable   : Memory → Ptr → Nat → Prop
LiveObject : Memory → Ptr → Prop
CString    : Memory → Ptr → Prop
Owns       : Memory → Owner → Ptr → Prop
Disjoint   : PtrRange → PtrRange → Prop
```

Every modeled load/store and library memory operation should expose the
necessary proof obligations through these predicates.

## Library and POSIX contracts

Do not initially formalize complete libc implementations. Give each used
operation a precise contract.

### `malloc`

Success returns a fresh live allocation of at least the requested size. Failure
returns null without modifying existing allocations.

### `realloc`

Success/failure behavior and ownership transfer must be modeled precisely. A
failed non-zero-size `realloc` preserves the original allocation.

### `strlen`

Requires a readable NUL-terminated byte string and returns the first NUL offset.

### `memcpy`

Requires readable source, writable destination and non-overlap as required by C.

### `memmove`

Requires readable source and writable destination but permits overlap.

### `snprintf`

Model enough behavior to prove bounds and termination for the fixed format
strings used by `wall-c`.

### POSIX/network operations

Socket, file, environment and time calls may initially be nondeterministic
operations satisfying explicit memory contracts. Kernel/network correctness is
outside the source memory-safety theorem.

## Function proof order

### 1. `build_magic_packet`

First complete end-to-end proof.

Preconditions:

- `mac` provides at least 6 readable bytes;
- `packet` provides at least 102 writable bytes.

Postconditions:

- only that destination region is modified;
- bytes 0..5 are `0xff`;
- bytes 6..101 are sixteen repetitions of the six MAC bytes;
- every access is in bounds.

### 2. MAC validation pipeline

Prove contracts for `validate_mac`, `normalize_mac` and `parse_mac`.
Caller-only assumptions must be stated explicitly rather than hidden in the
model.

### 3. Configuration ownership

Define invariants for `mac_list_t` and `target_list_t` and prove:

- successful append preserves them;
- failed `realloc` cannot lose the previous allocation;
- ownership transfer happens exactly once;
- partial failures release what they own;
- list cleanup frees every owned object exactly once.

### 4. Input parsing

Prove all stack/input buffer accesses and copies remain bounded, including
truncation/error paths.

### 5. Engine

Compose validation, parse and packet contracts. Establish exact stack-array
capacities at every call site.

### 6. Network/probe/CLI boundaries

Prove local memory safety while treating external calls under explicit POSIX
contracts.

### 7. `main`

Compose all paths and prove whole-program memory safety plus the desired cleanup
properties.

## Expected proof-driven C hardening

Formalization is allowed to reveal small C changes. Such findings should be
separate, reviewable commits with tests.

Known obligations include:

1. **Null/string preconditions** — some helpers call string functions before
   checking pointer arguments. Either prove callers establish validity or add a
   defensive check.

2. **Allocation-size overflow** — expressions such as
   `sizeof(T) * (count + 1)` need a mathematical bound or an explicit runtime
   overflow guard before `realloc`.

3. **Parser contracts** — helpers such as MAC parsing assume minimum readable
   input length and destination capacity. Make that contract explicit and prove
   it at every caller.

4. **CompCert compatibility** — any source construct outside the supported
   CompCert subset should be simplified where doing so does not degrade the
   program.

## Validation alongside proof

Keep existing dynamic validation:

```sh
make
make test
make test-integration
make sanitize
```

The Nix environment should eventually add:

```sh
nix flake check
nix build
nix develop .#proof
```

Finite vectors generated from Lean specifications may be compared against the C
binary as a differential sanity check. Differential testing is not the proof.

## Trusted computing base

For the Lean source theorem, expected TCB/assumptions include:

- Lean 4 kernel;
- C parser/importer unless proved correct;
- formal C-fragment semantics;
- explicit libc/POSIX contracts;
- source-byte/hash binding.

For the compiled-artifact claim add, as applicable:

- correctness assumptions in the Lean↔CompCert semantic bridge;
- CompCert's own stated trusted base and unverified front/back seams;
- assembler;
- linker;
- target sysroot/runtime;
- Nix and the pinned build inputs as reproducibility infrastructure.

Do **not** silently claim kernel, libc implementation, hardware or ordinary
GCC/Clang correctness.

Run `#print axioms` on top-level Lean theorems and document all remaining
non-standard assumptions.

## Source and build binding

A green proof against stale source is invalid.

The proof/build system should:

1. hash every verified `src/*.c` and relevant header;
2. parse exactly those bytes;
3. expose hashes as proof metadata;
4. fail if source differs from the proved AST;
5. pin all Nix inputs in `flake.lock`;
6. record CompCert version and relevant flags;
7. hash generated assembly;
8. hash final executable artifacts;
9. optionally reproduce the build independently and compare hashes.

## Milestones

### M0 — reproducible toolchain skeleton

- Commit `flake.nix`.
- Generate and commit `flake.lock` on a machine with Nix.
- Verify `nix flake check` on supported hosts.
- Confirm `ccomp`, `lean`, `lake`, GCC/Clang and current tests are reachable
  through the environment.

Exit criterion: toolchain versions are pinned and reconstructible.

### M1 — proof skeleton

- Create Lean project under `proof/`.
- Pin Lean dependencies.
- Define byte-addressable memory/allocation validity.
- Encode enough statements/loops/calls for `packet.c`.

Exit criterion: Lean checks a non-trivial memory-safety lemma.

### M2 — first actual C function

- Mechanically import `build_magic_packet`.
- Prove its exact 102-byte footprint and packet layout.

Exit criterion: theorem is attached to the parsed C function, not merely a Lean
rewrite.

### M3 — validation pipeline

Verify normalization, canonical validation and MAC parsing; land narrowly scoped
hardening patches where appropriate.

### M4 — heap ownership

Verify `config.c` allocation and cleanup logic, including integer-overflow
obligations and every allocation-failure path.

### M5 — whole runtime

Verify remaining CLI, network, probe and engine behavior under explicit external
contracts.

### M6 — whole-program Lean theorem

- prove top-level source memory safety;
- prove required owned-heap cleanup properties;
- record `#print axioms`;
- bind the theorem to source hashes.

### M7 — CompCert semantic bridge

- determine exact overlap between the Lean-modeled C fragment and CompCert C;
- define or mechanically validate a translation/correspondence relation;
- prove that the Lean safety theorem applies to the representation fed to
  CompCert;
- compile to assembly with the pinned `ccomp`.

Exit criterion: the source theorem and CompCert input are connected by an
explicit checked argument rather than human inspection.

### M8 — reproducible compiled artifact

- pin preprocessing strategy;
- pin assembler/linker/sysroot;
- compare assembly and executable hashes across independent Nix builds;
- document every remaining unverified seam.

This milestone produces a strong compiled-artifact story, but it must still be
worded according to the exact assumptions actually discharged.

## Success criterion

For the source theorem, an acceptable final statement is:

> For the pinned `wall-c` C source, under the stated C-library and POSIX
> contracts, Lean 4 proves that every modeled execution is free of memory
> undefined behavior; owned heap storage is also proved to be released on the
> specified termination paths.

For a later compiled-artifact statement, additionally require an explicit
Lean↔CompCert correspondence argument and a pinned reproducible build chain.
Anything weaker must be described more narrowly.