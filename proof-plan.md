# Lean 4 / CompCert proof-of-concept plan for `wall-c`

## Objective

`wall-c` is the proof-of-concept target. Before attempting a harder C program, demonstrate an auditable chain from exact C source bytes to a Lean 4 theorem, then toward CompCert and a reproducible Nix artifact.

The codebase should stay deliberately small while this work proceeds. Configuration-file support was removed before verification because it added file discovery, parsing, dynamic target lists, ownership transfer, and error paths without being essential to Wake-on-LAN packet sending.

## First proof target

Start with the actual implementation of:

```c
void build_magic_packet(const unsigned char *mac, unsigned char *packet);
```

Desired theorem, conceptually:

```lean
theorem build_magic_packet_safe_and_correct
    (m0 m1 : Memory)
    (mac packet : Ptr)
    (hmac : Readable m0 mac 6)
    (hpacket : Writable m0 packet 102)
    (hexec : ExecCFunction buildMagicPacketC m0 [mac, packet] m1) :
    MemorySafeExecution hexec ∧
    CorrectMagicPacket m0 m1 mac packet
```

The theorem must establish:

- only six input bytes are required;
- exactly 102 destination bytes are sufficient;
- every read and write is in bounds;
- bytes 0..5 are `0xff`;
- bytes 6..101 are sixteen repetitions of the six MAC bytes;
- memory outside the destination region is unchanged;
- the theorem is attached to a mechanically imported representation of the exact C source, not only a handwritten Lean equivalent.

## Proof-of-concept gate

Do not expand the verification scope until all four layers work.

### A. Source binding

- hash the exact verified C/header bytes;
- mechanically derive the proof input from those bytes;
- make proof CI fail closed when verified source changes;
- record source hashes with the proof result.

### B. Lean theorem

- mechanically import `build_magic_packet`;
- define only the C fragment required by that function;
- prove memory safety and packet correctness;
- record `#print axioms` for top-level theorems;
- document every non-standard assumption.

### C. CompCert bridge

- pin the CompCert version and flags;
- define a checked correspondence between the representation proved in Lean and the C/Clight representation compiled by CompCert;
- avoid a human-inspection-only semantic seam;
- hash generated assembly.

### D. Reproducibility

- commit `flake.lock`;
- pin compiler, assembler, linker, sysroot, Lean, and proof dependencies as applicable;
- reproduce assembly/artifact hashes in independent locked builds;
- state reproducibility separately from semantic correctness.

## Claims that must remain separate

1. **Lean source theorem:** the imported/modelled C execution satisfies the proved safety/correctness property.
2. **Compiler preservation:** CompCert preserves the relevant source semantics through its verified passes.
3. **Reproducibility:** identical pinned inputs produce identical bits.

Nix helps with 3. CompCert helps with 2. Lean is used for 1 and for the correspondence argument needed between our source representation and CompCert.

## Minimal Lean architecture

Do not design a general C-verification framework before the first theorem exists.

```text
proof/
  WallC/
    Memory.lean
    Syntax.lean
    Semantics.lean
    Source.lean
    Hoare.lean

    LibC/
      MemoryOps.lean
      CString.lean

    Wall/
      Packet.lean
      Validate.lean
      Cli.lean
      Net.lean
      Probe.lean
      Engine.lean
      Main.lean

    Bridge/
      SourceHashes.lean
      CompCert.lean

    Theorems.lean
```

For `build_magic_packet`, the required C fragment is intentionally tiny: byte pointers, integer arithmetic, pointer offsets, a bounded `for` loop, calls, `memset`, and `memcpy`. Unsupported syntax must fail import loudly.

## Whole-program phase

After the first end-to-end function succeeds, expand toward:

```lean
theorem wall_c_memory_safe
    (input : ProcessInput)
    (s0 s1 : CState)
    (hinput : ValidProcessInput input)
    (hexec : Exec wallCProgram input s0 s1) :
    ¬ HasMemoryUB s1
```

`HasMemoryUB` should cover at least out-of-bounds access, invalid/null dereference, use-after-free, invalid free, uninitialized reads where undefined, illegal overlap, and modeled lifetime/provenance violations.

The simplified runtime proof order should be:

1. `packet.c` — fixed memory footprint and functional correctness;
2. `validate.c` — MAC normalization/validation and parsing contracts;
3. `cli.c` — bounded stdin line handling and confirmation behavior;
4. `engine.c` — composition of validation, packet construction, repeats and output;
5. `net.c` — socket/address memory safety under explicit POSIX contracts;
6. `probe.c` — platform-specific probe parsing and the remaining dynamic allocation on macOS;
7. `main.c` — CLI control-flow composition and top-level theorem.

The project no longer needs to prove a config parser, dynamic target-list ownership, XDG/HOME config discovery, or `realloc`-grown target arrays.

## External contracts

Do not formalize entire libc or kernels initially. Give used operations explicit contracts, including:

- `memset`, `memcpy`, `strlen`, `snprintf`, `sscanf`;
- `fgets` for the one-line stdin input path;
- socket/address operations;
- interface enumeration/resolution;
- Linux `/proc/net/arp` or macOS routing-table interfaces used by smart probing;
- allocation/free used by the macOS probe path.

Kernel, libc implementation, network hardware, and remote-host correctness remain outside the source theorem unless separately modeled.

## Milestones

### M0 — clean and pin the target

- keep the C runtime minimal;
- remove nonessential parsing/state surfaces;
- generate and commit `flake.lock`;
- confirm locked Lean and CompCert tooling is reconstructible.

### M1 — first mechanically imported C function

- import exact `packet.c` source;
- bind it to source hashes;
- establish the minimal memory model and `memset`/`memcpy` contracts.

### M2 — first kernel-checked theorem

- prove `build_magic_packet` memory safe;
- prove exact packet layout;
- record axioms/assumptions.

### M3 — CompCert correspondence

- connect the Lean-proved representation to the CompCert input semantics;
- compile the pinned source with pinned CompCert;
- hash assembly.

### M4 — reproducible artifact

- pin remaining build inputs;
- compare independent build hashes;
- document all residual unverified seams.

### M5 — whole-program `wall-c`

Expand module-by-module until the top-level memory-safety theorem is established for the pinned simplified source.

## Success criterion

The concept is proved only when a reviewer can start from exact `wall-c` source bytes and follow a mechanically checked path to a Lean theorem, with a clearly delimited CompCert bridge and reproducible build story. Anything weaker must be described more narrowly.
