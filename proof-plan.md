# Lean 4 memory-safety proof plan

## Status

This document is an engineering/research plan, not a claim that `wall-c` has
already been formally verified.

The initial code target is the `main` source tree at commit
`c3d4c2322425a013e182ea79804bdc059a104b25`. The proof machinery should pin the
exact source revision (or source-file hashes) it verifies so that a theorem
cannot silently drift away from the C program.

## Goal

Prove, in Lean 4, that the actual `wall-c` C99 source is memory safe for all
executions satisfying explicit platform and library contracts.

The intended top-level result is conceptually:

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
- invalid or null pointer dereferences;
- use-after-free;
- double-free and invalid free;
- reads of uninitialized storage where C makes them undefined;
- invalid overlap for operations whose contracts forbid overlap;
- allocation-size arithmetic overflow that could produce undersized objects;
- lifetime/provenance violations represented by the chosen C memory model.

Memory leaks should be proved separately because a leak is not, by itself,
undefined behavior:

```lean
theorem wall_c_no_owned_heap_leaks_on_normal_exit :
  NormalTermination e → AllWallOwnedAllocationsReleased e
```

## Non-goals

The first complete proof does **not** need to prove:

- correctness of the Linux or macOS kernels;
- correctness of libc implementations;
- correctness of network hardware;
- correctness of GCC/Clang machine-code generation;
- liveness of remote Wake-on-LAN targets;
- absence of all classes of C undefined behavior unrelated to memory unless
  they are needed by the memory-safety argument.

These boundaries must remain explicit in the final theorem and documentation.

## Required proof strength

There are three useful levels, but only level 2 is the target of this project.

### Level 1: model proof

Reimplement the relevant algorithms as pure Lean functions and prove those
functions safe/correct.

Useful for discovering invariants, but insufficient as the final claim because
it leaves an unproved correspondence between the Lean model and the C source.

### Level 2: source proof — target

Parse the actual `wall-c` C source into a mechanically obtained AST, execute or
reason about that AST using formal C semantics in Lean, and prove memory safety
of that program.

No hand-written Lean translation of a C function may be the sole bridge for the
final theorem.

### Level 3: binary proof

Connect the source theorem to optimized machine code through a verified or
otherwise justified compiler path.

This is deliberately deferred. The normal `wall-c` release build currently
uses an optimizing C compiler, so a source-level theorem alone must not be
presented as a theorem about arbitrary produced binaries.

## Why `wall-c` is a suitable target

The program has several properties that keep the verification problem small:

- a small number of C99 translation units;
- sequential control flow;
- no threads;
- explicit heap ownership helpers;
- small fixed-size stack buffers in the packet path;
- very limited pointer arithmetic;
- bounded packet construction;
- small POSIX/networking boundary;
- existing unit, integration, ASan and UBSan testing that can serve as a
  differential sanity check while the formal model is built.

The highest-value proof work is expected to be in `src/config.c`, where dynamic
allocation, `realloc`, list growth and ownership transfer occur.

## Proof architecture

The proof should be layered so that the trusted assumptions are visible.

```text
actual wall-c C source
        |
        v
mechanical C parser / AST
        |
        v
formal C fragment semantics
        |
        +--> libc contracts
        |
        +--> POSIX/network contracts
        |
        v
per-function Hoare-style proofs
        |
        v
module invariants / ownership proofs
        |
        v
whole-program memory-safety theorem
```

A proposed Lean tree:

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

    Theorems.lean
```

The exact layout may change as the semantics become concrete.

## C fragment to model

Do not attempt to formalize all of ISO C before proving `wall-c`.

Start with the fragment actually exercised by this program:

- integer and character scalar types;
- arrays and structs;
- pointers and null;
- stack and heap objects;
- address-of and dereference;
- field and array access;
- pointer addition used by the program;
- assignments;
- conditionals;
- bounded and data-dependent loops;
- function calls and returns;
- casts used in the source;
- `sizeof`;
- relevant integer-conversion behavior;
- allocation failure paths.

Unsupported syntax must fail the proof import rather than be silently ignored.

## Memory model

The model must track at least:

```lean
structure Allocation where
  base      : Addr
  size      : Nat
  live      : Bool
  writable  : Bool
  initialized : Finset Nat
```

The real representation may be more sophisticated, especially if pointer
provenance is modeled explicitly.

Useful predicates include:

```lean
Readable      : Memory → Ptr → Nat → Prop
Writable      : Memory → Ptr → Nat → Prop
LiveObject    : Memory → Ptr → Prop
CString       : Memory → Ptr → Prop
Owns          : Memory → Owner → Ptr → Prop
Disjoint      : PtrRange → PtrRange → Prop
```

Every modeled load, store, `memcpy`, `memmove`, string operation and allocation
operation should expose proof obligations through these predicates.

## Library boundary

Do not formalize complete libc implementations initially. Give the functions
used by `wall-c` explicit contracts.

Examples:

### `malloc`

On success, returns a fresh live allocation of at least the requested size. On
failure, returns null and leaves existing allocations unchanged.

### `realloc`

The contract must distinguish success and failure precisely. In particular, a
failed non-zero-size `realloc` preserves ownership and validity of the original
allocation.

### `strlen`

Requires a readable NUL-terminated byte string and returns the offset of the
first NUL.

### `memcpy`

Requires readable source, writable destination and the C-required overlap
condition.

### `memmove`

Requires readable source and writable destination but permits overlap.

### `snprintf`

Model enough of the function to prove destination bounds and termination for
the format strings used by `wall-c`.

### POSIX/network calls

Socket, environment, file and time functions may initially be nondeterministic
operations satisfying explicit memory contracts. Their external behavior is
not part of the memory-safety theorem unless required to establish safety.

## Function proof order

### 1. `build_magic_packet`

This should be the first end-to-end proof.

Preconditions:

- `mac` points to at least 6 readable bytes;
- `packet` points to at least 102 writable bytes.

Postconditions:

- only the 102-byte destination region is modified;
- bytes 0..5 are `0xff`;
- bytes 6..101 are sixteen repetitions of the six MAC bytes;
- all reads and writes are in bounds.

This proof validates the basic memory model, loops and `memcpy` contract.

### 2. MAC validation and normalization

Prove contracts for:

- `validate_mac`;
- `normalize_mac`;
- `parse_mac`.

Important distinction: safety may depend on caller preconditions even when the
function is not locally defensive. The proof must state those preconditions
rather than smuggling them in through the model.

Where a tiny C change can make a public helper locally total or substantially
reduce its contract, prefer considering that patch separately.

### 3. Configuration ownership

Formalize invariants for `mac_list_t` and `target_list_t`.

Conceptually:

```lean
structure TargetListInv (m : Memory) (xs : TargetListView) : Prop where
  backing_live     : ...
  backing_capacity : ...
  members_live     : ...
  strings_owned    : ...
  strings_disjoint : ...
```

Prove that:

- `append_target` preserves the invariant on success;
- allocation failure does not lose the original list;
- ownership transferred from a temporary target is represented exactly once;
- all partial-construction error paths release everything they own;
- `free_target_list` releases all list-owned allocations exactly once;
- analogous properties hold for `mac_list_t`.

### 4. Input parsing

Prove that fixed-size input and config buffers are never indexed or copied
outside their bounds, including truncation/error paths.

### 5. Engine

Compose normalization, parsing and packet construction contracts. Prove the
stack arrays have the exact capacities required by their callees and that all
pointer arguments passed through the engine satisfy those contracts.

### 6. Network, probe and CLI boundaries

Prove local memory safety around system calls while treating the calls
nondeterministically under their POSIX memory contracts.

### 7. `main`

Establish initial invariants, compose all paths, and prove that every normal or
error exit leaves no live allocation owned by `wall-c`.

## Expected hardening findings

Formalization should be allowed to produce small C patches. These are findings,
not failures of the proof effort.

Known proof obligations worth examining immediately include:

1. **Null/string preconditions**

   Some helpers call string functions before checking their pointer arguments.
   Either the caller invariant must prove these pointers valid, or the helper
   should gain a defensive check.

2. **Allocation-size overflow**

   Expressions of the form:

   ```c
   sizeof(T) * (count + 1)
   ```

   need a proof that `count + 1` and the multiplication cannot wrap `size_t`, or
   an explicit runtime guard before `realloc`.

3. **Parser contracts**

   Helpers such as MAC parsing rely on minimum readable input length and
   destination capacity. Make those assumptions explicit and determine whether
   callers establish them on every path.

Each hardening patch should be a separate reviewable commit with tests. The
proof must target the post-patch source hash once such changes land.

## Validation alongside Lean

Formal proof should coexist with the existing dynamic checks rather than
replace them.

During development, keep running:

```sh
make
make test
make test-integration
make sanitize
```

Where useful, generate finite test vectors from Lean specifications and compare
them against the C binary. Differential tests do not constitute the proof, but
they are valuable for detecting mistakes in the semantics or correspondence
layer.

## Trusted computing base

The final result must contain a short, auditable TCB statement.

Expected TCB for the source-level theorem:

- Lean 4 kernel;
- the C parser/importer unless its correctness is itself proved;
- the formal C-fragment semantics;
- explicit libc/POSIX contracts;
- the mechanism binding the theorem to exact source bytes.

Items **not** silently included in the theorem:

- GCC or Clang correctness;
- operating-system correctness;
- libc implementation correctness beyond the stated contracts;
- network-stack correctness.

The project should periodically run `#print axioms` on its top-level theorems
and document any non-standard axioms that remain.

## Source binding

A proof about stale source is not useful.

The proof build should eventually:

1. hash every verified `src/*.c` and relevant header;
2. parse exactly those files;
3. expose the hashes as proof-build metadata;
4. fail if the source changes without regenerating/rechecking the proof input.

A CI job should reject a green "verified" status whenever the C source and the
proved AST are out of sync.

## Milestones

### M0 — proof skeleton

- Create Lean project under `proof/`.
- Pin Lean/toolchain versions.
- Define byte-addressable memory and allocation validity.
- Encode enough statements/loops/calls for `packet.c`.

Exit criterion: Lean checks a non-trivial memory-safety lemma over the model.

### M1 — first actual C function

- Mechanically import `build_magic_packet`.
- Prove its exact 102-byte memory footprint and functional packet layout.

Exit criterion: theorem is attached to the parsed C function, not a handwritten
Lean rewrite.

### M2 — validation pipeline

- Verify normalization, canonical validation and MAC parsing.
- Patch any unacceptable caller-only safety assumptions if justified.

Exit criterion: engine's MAC-to-packet path composes from proved contracts.

### M3 — heap ownership

- Verify `config.c` allocation and cleanup logic.
- Add explicit overflow guards if the proof requires them.

Exit criterion: success and all allocation/error paths preserve ownership
invariants and cannot double-free or leak owned objects.

### M4 — whole runtime

- Add remaining CLI, network, probe and engine proofs.
- State POSIX boundary contracts explicitly.

Exit criterion: all `src/*.c` paths required by the executable are represented.

### M5 — whole-program theorem

- Prove top-level memory safety.
- Prove no owned heap leaks on normal/error program termination as appropriate.
- Record `#print axioms` output.
- Bind proof to exact source hashes.

Exit criterion: CI produces an auditable proof result for the pinned C source.

### M6 — compiler bridge, optional research extension

Investigate connecting the source proof to machine code through a verified
compiler or translation-validation approach.

This milestone must remain separate so that the source-level result is not
oversold.

## Success criterion

The experiment succeeds when a reviewer can start from the exact C source,
follow a mechanically defined path into Lean, and reach a kernel-checked theorem
whose assumptions explicitly delimit the environment.

The strongest acceptable short description would then be:

> For the pinned `wall-c` C source, under the stated C-library and POSIX
> contracts, Lean 4 proves that every modeled execution is free of memory
> undefined behavior; owned heap storage is also proved to be released on the
> specified termination paths.

Anything weaker should be described more narrowly.