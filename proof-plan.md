# Lean 4 / CompCert proof-of-concept plan for `wall-c`

## Status

This is an engineering/research plan, not a claim that `wall-c` is already
formally verified.

`wall-c` is deliberately the **first and only verification target for this
phase**. The objective is to prove that an end-to-end method can work on a small,
real C99 program before applying it anywhere harder.

The project should resist scope expansion until the proof-of-concept gate below
has been crossed.

The proof must bind itself to exact source bytes. Any source change that affects
verified code must invalidate the corresponding proof artifact until it is
re-imported and rechecked.

## Proof-of-concept objective

The first objective is **not** whole-program verification.

The first objective is to take one actual C function from `wall-c` all the way
through this chain:

```text
exact wall-c source bytes
        |
        | hash / source binding
        v
mechanical C import
        |
        v
formal Lean 4 representation + semantics
        |
        v
kernel-checked safety + functional theorem
        |
        | checked semantic correspondence
        v
CompCert input representation
        |
        v
CompCert verified compiler core
        |
        v
assembly
        |
        | pinned assembler / linker / sysroot where applicable
        v
reproducible artifact under Nix
```

The chosen first function is:

```c
void build_magic_packet(const unsigned char *mac, unsigned char *packet);
```

It is ideal because it has:

- a fixed 6-byte input;
- a fixed 102-byte output;
- one bounded loop;
- `memset` and `memcpy` as the only relevant library memory operations;
- a simple functional specification;
- no allocation, files, sockets, environment, or error-handling complexity.

If we cannot complete the method cleanly on this function, we should not expand
the proof scope.

## First theorem target

The first source-bound theorem should establish both memory safety and packet
correctness.

Conceptually:

```lean
theorem build_magic_packet_safe_and_correct
    (m0 m1 : Memory)
    (mac packet : Ptr)
    (hmac : Readable m0 mac 6)
    (hpacket : Writable m0 packet 102)
    (hexec : ExecCFunction buildMagicPacketC m0 [mac, packet] m1) :
    MemorySafeExecution hexec ∧
    PacketRegion m1 packet =
      (Array.replicate 6 0xff) ++
      Array.flatten (Array.replicate 16 (ReadBytes m0 mac 6))
```

The exact theorem shape will depend on the chosen C semantics, but the claim
must include at least:

- exactly six readable input bytes are sufficient;
- exactly 102 writable output bytes are sufficient;
- every C read/write performed by the function is in bounds;
- the first six output bytes are `0xff`;
- the following 96 bytes are sixteen repetitions of the input MAC;
- no memory outside the destination region is modified;
- the theorem is attached to a mechanically imported representation of the
  actual C source, not merely a handwritten Lean equivalent.

## Proof-of-concept gate

The proof of concept is complete only when **all** of the following are true.

### A. Source binding

- the exact source file bytes are hashed;
- the imported C representation is generated from those bytes;
- CI fails if verified source changes without regenerating/rechecking the proof;
- the theorem cannot silently continue proving stale code.

### B. Lean theorem

- `build_magic_packet` is mechanically imported;
- Lean proves its memory safety under explicit pointer-size preconditions;
- Lean proves the 102-byte packet layout;
- `#print axioms` is recorded for the top-level theorem;
- all non-standard assumptions are documented.

### C. Compiler bridge

- the representation proved in Lean is explicitly related to the C program fed
  to CompCert;
- that relation is mechanically checked or proved, not asserted by human
  inspection;
- the exact CompCert version and flags are pinned;
- generated assembly is hashed.

### D. Reproducibility

- `flake.lock` pins all Nix inputs;
- the relevant build succeeds from the flake;
- two independent builds from identical locked inputs produce identical
  assembly/artifact hashes, or any unavoidable nondeterminism is isolated and
  documented;
- reproducibility is described separately from semantic correctness.

Only after A–D succeed should the project expand toward proving the rest of
`wall-c`.

## Claims we must keep separate

Three properties are related but not interchangeable:

1. **Lean source theorem** — the modeled/imported C execution is memory safe and
   functionally correct under stated contracts;
2. **compiler correctness** — CompCert preserves the relevant source semantics
   through its verified compiler passes;
3. **reproducible build** — identical pinned build inputs produce identical
   output bits.

Nix helps with 3. CompCert helps with 2. Lean is used for 1 and for the bridge we
need between our source representation and CompCert's semantics.

A reproducible wrong compiler would still produce the same wrong binary.
A verified source theorem with an unchecked source-import bridge would still be
weaker than desired. These boundaries must remain visible.

## Why `wall-c`

`wall-c` is a good proof-of-concept target because it is small but not synthetic:

- ordinary C99;
- multiple translation units;
- fixed-size stack buffers;
- dynamic allocation and ownership in `config.c`;
- string parsing;
- libc usage;
- POSIX files/environment;
- networking;
- explicit error paths;
- existing GCC/Clang, sanitizer and integration testing;
- no threads.

It lets us begin with an extremely small function and then increase proof
complexity without changing projects.

## Scope after the proof-of-concept gate

If the first end-to-end function succeeds, the next goal is whole-program
`wall-c` memory safety.

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

- out-of-bounds reads/writes;
- invalid/null dereferences;
- use-after-free;
- double-free / invalid free;
- uninitialized reads where C makes them undefined;
- invalid overlap for operations such as `memcpy`;
- allocation-size arithmetic overflow producing undersized objects;
- lifetime/provenance violations represented by the chosen model.

Heap leaks are a separate property:

```lean
theorem wall_c_no_owned_heap_leaks_on_normal_exit :
  NormalTermination e → AllWallOwnedAllocationsReleased e
```

Do not conflate memory safety with absence of leaks.

## Non-goals for this repository phase

Until the `wall-c` proof of concept and, ideally, the whole-program theorem are
complete, do not expand this repository into another application's verification
workspace.

The current phase does not attempt to prove:

- the operating-system kernel;
- correctness of libc implementations beyond explicit contracts;
- network hardware or remote Wake-on-LAN behavior;
- arbitrary GCC/Clang correctness;
- absence of hardware faults;
- all possible classes of C undefined behavior unless required by the stated
  theorem.

Future verification targets belong in separate projects after this method has
been demonstrated.

## Lean architecture

The proof code should be generic where practical, but its only immediate client
is `wall-c`.

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
      MemoryOps.lean
      CString.lean
      Alloc.lean
      Stdio.lean
      Parse.lean

    Posix/
      Environment.lean
      Socket.lean
      Time.lean

    Wall/
      Packet.lean
      Validate.lean
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

Do not over-generalize the framework before the first theorem exists. Reusable
abstractions should be extracted only when the `wall-c` proof demonstrates a
real need.

## C fragment strategy

Do not formalize ISO C wholesale.

For the first theorem we only need a very small fragment:

- function arguments;
- byte pointers;
- integer loop variable;
- pointer arithmetic used by `packet.c`;
- function calls;
- `memset`;
- `memcpy`;
- bounded `for` loop;
- integer arithmetic required for offsets.

Unsupported syntax must fail import loudly.

As verification expands, add only the constructs exercised by the next
`wall-c` module.

## Memory model

The model must at least represent:

- live allocations/objects;
- object size;
- readability/writability;
- initialization state where relevant;
- pointer offset validity;
- enough provenance/lifetime information to make the selected memory-safety
  theorem meaningful.

Useful predicates include:

```lean
Readable   : Memory → Ptr → Nat → Prop
Writable   : Memory → Ptr → Nat → Prop
LiveObject : Memory → Ptr → Prop
CString    : Memory → Ptr → Prop
Owns       : Memory → Owner → Ptr → Prop
Disjoint   : PtrRange → PtrRange → Prop
```

For the first proof, `Readable`, `Writable`, pointer offsets and modified-region
reasoning are sufficient. Avoid designing the entire eventual heap model before
that theorem checks.

## Library contracts for the first proof

### `memset`

Given a writable destination range of length `n`, writes exactly `n` bytes and
leaves memory outside that range unchanged.

### `memcpy`

Given a readable source range and writable non-overlapping destination range of
length `n`, copies exactly those bytes and leaves memory outside the destination
range unchanged.

These contracts must be explicit theorem assumptions or proved library lemmas,
not informal comments.

## CompCert boundary

CompCert does not remove the need to define our trust boundary.

The project must identify separately:

- preprocessing;
- C parsing/front-end representation;
- the verified compiler core;
- assembly;
- linking;
- runtime/sysroot.

The hard research question for the PoC is the **Lean semantics ↔ CompCert C
semantics** relationship.

The bridge must answer:

> Is the exact program Lean proved the same program, in the relevant semantic
> sense, that CompCert compiles?

Until that question has a checked answer, the Lean theorem and the CompCert
compiler theorem remain adjacent results rather than one end-to-end proof.

## Nix/reproducibility plan

`flake.nix` defines the desired environment; `flake.lock` is the actual immutable
input pin.

The environment should pin or expose:

- Lean 4 / Lake;
- CompCert;
- GCC and Clang for differential testing;
- the ordinary build dependencies;
- build environment variables that would otherwise inject timestamps, paths or
  host-specific state.

Eventually CI should perform an independent rebuild/hash comparison.

Do not claim reproducibility before the lockfile is committed and the comparison
has actually passed.

## Existing dynamic validation

Formal work should preserve the ordinary engineering checks:

```sh
make
make test
make test-integration
make sanitize
```

and later:

```sh
nix flake check
nix build
nix develop .#proof
```

Sanitizers and differential tests are useful oracles for proof/model mistakes,
but they are not evidence replacing the theorem.

## Proof-driven C hardening

The proof may reveal places where a tiny C change substantially simplifies or
strengthens the contract. Those changes are legitimate findings and should land
as separate reviewable commits with tests.

Known later obligations include:

1. null/string preconditions around helpers such as validation/parsing;
2. `size_t` overflow in expressions such as `sizeof(T) * (count + 1)` before
   `realloc`;
3. minimum readable string length assumptions in MAC parsing;
4. precise ownership transfer and cleanup on allocation failures;
5. any source construct that blocks the selected CompCert subset without adding
   meaningful program value.

Do not modify `build_magic_packet` merely to make the first theorem artificially
easy unless the patch is independently defensible C engineering.

## Trusted computing base

For the initial Lean theorem, expected trusted components/assumptions include:

- Lean 4 kernel;
- the C importer/parser unless its correctness is itself proved;
- the formal C-fragment semantics;
- explicit `memcpy` / `memset` contracts;
- source-byte/hash binding mechanism.

For later compiled-artifact claims add, where applicable:

- any assumptions in the Lean↔CompCert bridge;
- CompCert's documented trusted/unverified seams;
- assembler;
- linker;
- runtime/sysroot;
- Nix/build machinery as reproducibility infrastructure.

Every top-level theorem should have `#print axioms` recorded.

## Milestones

### P0 — deterministic workspace

- keep CI green;
- generate and commit `flake.lock` on a Nix-capable machine;
- run `nix flake check`;
- confirm pinned `lean`, `lake` and `ccomp` versions;
- confirm CompCert accepts the relevant `wall-c` subset.

Exit criterion: the tool environment is reconstructible.

### P1 — mechanical source import

- create the Lean project;
- import the actual `src/packet.c` function mechanically;
- bind the imported representation to exact source bytes/hashes;
- make unsupported syntax fail closed.

Exit criterion: changing the C function invalidates/regenerates the proof input.

### P2 — first source theorem

- implement only the memory semantics needed by `build_magic_packet`;
- formalize `memset` and `memcpy` contracts;
- prove in-bounds execution;
- prove exact packet layout;
- record `#print axioms`.

Exit criterion: a kernel-checked theorem refers to the mechanically imported C
function.

### P3 — CompCert bridge

- compile the pinned source with pinned CompCert;
- identify the exact CompCert representation at the source semantic boundary;
- define and prove/check correspondence with the Lean representation;
- record generated assembly hash.

Exit criterion: the Lean theorem and CompCert input are connected by a checked
argument, not by visual comparison.

### P4 — reproducibility

- pin preprocessing strategy, assembler/linker/sysroot where used;
- rebuild independently from the same `flake.lock`;
- compare assembly and final artifact hashes;
- document any residual nondeterminism or unverified seams.

Exit criterion: all four PoC-gate sections A–D are satisfied.

## PoC decision point

At P4, stop and assess the result before doing more proof work.

Questions:

- Is the source-import path small and auditable?
- Is the C semantics model credible rather than tailored to one happy path?
- Is the Lean↔CompCert bridge maintainable?
- Is the TCB acceptably small and explicit?
- Does a C source edit reliably invalidate the relevant proof?
- Is the cost per additional C function reasonable?

If the answer is no, redesign the method while the experiment is still small.

If yes, continue with the rest of `wall-c`.

## Whole-program milestones after PoC

### W1 — validation pipeline

Verify `validate_mac`, `normalize_mac` and `parse_mac`, including explicit caller
preconditions or defensible hardening patches.

### W2 — configuration ownership

Verify `config.c` heap/list invariants:

- safe `realloc` growth;
- overflow guards/proofs;
- ownership transfer exactly once;
- cleanup on every partial failure;
- no double-free/use-after-free;
- desired no-leak termination properties.

### W3 — remaining runtime

Verify CLI, engine, network and probe paths under explicit POSIX contracts.

### W4 — whole-program theorem

- prove whole-program memory safety;
- prove the selected heap-cleanup theorem;
- bind theorem to all relevant source hashes;
- record `#print axioms` and final TCB.

## Success criterion

The **proof-of-concept** succeeds when we can truthfully say:

> For the exact pinned `build_magic_packet` C source, Lean 4 mechanically checks
> memory safety and the exact packet layout; the proved representation is linked
> by an explicit checked correspondence to the CompCert input; and the resulting
> build artifacts are reproducible from pinned Nix inputs, with all remaining
> trusted seams documented.

Only after that claim is real should this verification method be treated as a
candidate for a larger target.

The **whole `wall-c`** success criterion is stronger:

> For the pinned `wall-c` C source, under the stated libc and POSIX contracts,
> Lean 4 proves every modeled execution free of the specified memory undefined
> behavior, with separate ownership/cleanup guarantees and an explicitly bounded
> compiler/build trust chain.
