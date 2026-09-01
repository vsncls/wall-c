# OpenBSD `doas` verification plan

## Status

This document incubates the `doas` verification effort inside `wall-c` while the
Lean 4 / C-semantics / CompCert infrastructure is still being developed here.

It is **not** part of the `wall-c` proof claim. Once the reusable verification
machinery is credible, this document and the relevant generic proof components
should move into a dedicated repository (tentatively `doas-verify`).

The point of keeping the plan here temporarily is to make `wall-c` serve as the
small engineering proving ground and `doas` as the security-critical target that
justifies the infrastructure.

## Upstream target and provenance

Canonical source project:

```text
OpenBSD src
path: usr.bin/doas/
GitHub read-only mirror: https://github.com/openbsd/src
```

The latest commit touching `usr.bin/doas/` observed when this plan was created:

```text
38599afa1d1d1f14a897b01350e8ce94486e1788
```

At that snapshot, the directory contains:

```text
Makefile
doas.1
doas.c
doas.conf.5
doas.h
env.c
parse.y
```

Observed blob identities:

```text
Makefile       f2e5529d6abcdb77b3f347630e21b2a6c4aeae18
doas.c         3999b2e2f64aaf23917f36d59cbb51b50cf18d93
doas.h         ce6a03618ac7e2596e6642aaff10e7ea61db8d17
env.c          2d93a4089b6b4ea71ec6c922c88f22b79d15a6d6
parse.y        604becb5445077766203a899c2923610e2745869
doas.1         25827cc7104bcc9447302684d8da5604acf143c7
doas.conf.5    c547366ab0b1b8e782523e4b840e2ace55f35c5c
```

The future dedicated repository should vendor an **unchanged** snapshot of this
subdirectory and record both the OpenBSD commit and per-file hashes. Upstream
sources should never be casually edited in place.

## Why `doas`

`doas` is a substantially more valuable formal-verification target than a normal
utility because it implements a privilege boundary.

Memory safety is necessary but not sufficient. The interesting failure modes are
also semantic:

- executing a command that policy should deny;
- selecting the wrong matching rule;
- authenticating when policy says `nopass`, or failing to authenticate when
  policy requires it;
- executing under the wrong target identity or supplementary groups;
- carrying an environment variable that policy should have removed;
- accepting an unsafe configuration file;
- weakening sandboxing before all attacker-controlled parsing is finished;
- reaching `exec` through a path that has not established all authorization
  invariants.

A successful project should therefore prove a **stack of security properties**,
not merely `doas_memory_safe`.

## Top-level proof goals

The theorem names below are sketches. Exact statements will depend on the final
C semantics and OpenBSD system-call model.

### 1. Memory safety

```lean
theorem doas_memory_safe :
  ValidInitialState s0 →
  Exec doasProgram input s0 trace s1 →
  MemorySafe trace
```

Covers at minimum:

- no out-of-bounds access;
- no invalid/null dereference;
- no use-after-free;
- no double/invalid free;
- no invalid string or buffer access;
- no allocation-size overflow leading to undersized objects;
- no modeled pointer-lifetime/provenance violation.

### 2. Authorization soundness

The central security theorem should resemble:

```lean
theorem exec_implies_authorized :
  ReachesExec trace target cmd argv →
  ∃ rule,
    SelectedRule config caller groups target cmd argv = some rule ∧
    rule.action = Permit
```

This must capture **the actual OpenBSD matching semantics**, including rule
ordering and the precise last/selected-match behavior in the source.

### 3. Authentication obligation

```lean
theorem privileged_exec_requires_auth_when_required :
  ReachesExec trace target cmd argv →
  SelectedPermitRule ... = some rule →
  ¬ rule.nopass →
  AuthenticationSucceeded trace caller
```

Authentication itself may initially be modeled as a trusted OpenBSD boundary;
the proof obligation is that `doas` invokes and checks it exactly when policy
requires it.

### 4. Target credential correctness

```lean
theorem exec_uses_requested_target_identity :
  ReachesExecState trace execState target cmd argv →
  execState.euid = target.uid ∧
  execState.egid = target.gid ∧
  CorrectSupplementaryGroups execState target
```

The exact credential properties should match OpenBSD `setusercontext` and
related calls rather than an invented Unix abstraction.

### 5. Environment confinement

```lean
theorem exec_environment_is_authorized :
  ReachesExecState trace execState target cmd argv →
  ∀ entry ∈ execState.env,
    AllowedEnvironmentEntry selectedRule caller target entry
```

This is likely one of the cleanest high-value component proofs because `env.c`
is small and explicitly constructs the environment passed to the final command.

### 6. Configuration-file trust

Prove that a configuration is used for authorization only after the required
ownership and write-permission checks have succeeded.

Conceptually:

```lean
theorem policy_use_requires_trusted_config :
  PolicyDecisionDerivedFrom cfg trace →
  RootOwned cfg ∧
  NotGroupWritable cfg ∧
  NotOtherWritable cfg
```

The exact statement must be derived from current OpenBSD source behavior.

### 7. Sandbox monotonicity

OpenBSD `pledge` / `unveil` transitions should be modeled as capabilities that
can only be reduced. Then prove that sensitive parsing/authorization stages do
not require or retain capabilities beyond their intended phase.

This is not merely a syscall-order test: the desired theorem should constrain
the abstract capability state along all executions.

## What should remain outside the first theorem

The first serious result should not claim to prove:

- the OpenBSD kernel;
- correctness of BSD Authentication itself;
- correctness of libc implementations;
- absence of hardware faults;
- correctness of the machine-code compiler unless a verified compiler bridge
  has actually been established;
- security of arbitrary programs executed by `doas`;
- security of a modified portable fork such as OpenDoas.

These are assumptions or later proof layers, not gaps to conceal.

## Relationship to the `wall-c` effort

`wall-c` should build and test only **generic infrastructure** that can later be
reused:

```text
wall-c
  |
  +-- source-byte binding / hashes
  +-- mechanical C import
  +-- C memory model
  +-- libc contracts
  +-- Hoare/separation-style proof layer
  +-- allocation ownership invariants
  +-- CompCert experiment
  +-- Nix reproducibility machinery
  |
  v
reusable verified-C substrate
  |
  v
doas-verify
  +-- OpenBSD-specific syscall contracts
  +-- parser semantics
  +-- policy semantics
  +-- authentication-state model
  +-- credential-transition model
  +-- environment proof
  +-- pledge/unveil capability model
```

Do **not** distort `wall-c` APIs or source code merely to make future `doas`
verification easier. Reuse should happen in proof infrastructure, not by forcing
unrelated C programs into one application design.

## Parser strategy

`parse.y` is an important boundary because policy correctness depends on the
configuration that the parser constructs.

Three possible approaches, in decreasing preference:

1. mechanically model/import the generated parser and prove correspondence to
   the policy AST;
2. prove a parser-equivalence theorem between the generated parser and a small
   Lean specification for `doas.conf`;
3. temporarily trust the parser and begin the security theorem from a
   well-formed parsed rule list.

Option 3 is acceptable only as an explicitly weaker intermediate milestone.
The final authorization theorem should not silently assume correct parsing.

## OpenBSD boundary model

The `doas` project will require contracts beyond the POSIX subset needed by
`wall-c`.

Expected boundaries include at least:

- passwd/group lookup;
- supplementary group discovery;
- BSD Authentication;
- tty/session authentication state where relevant;
- `setusercontext` / credential transition;
- `pledge`;
- `unveil`;
- configuration file metadata and permission checks;
- `execvpe` or the exact current exec path;
- filesystem/path lookup semantics relevant to command resolution.

Each boundary needs a contract that is strong enough to prove the `doas`
security theorem but no stronger than what OpenBSD actually guarantees.

## Proposed future repository

When extracted from `wall-c`:

```text
doas-verify/
  upstream/
    usr.bin/doas/
      Makefile
      doas.1
      doas.c
      doas.conf.5
      doas.h
      env.c
      parse.y

  proof/
    C/
    OpenBSD/
    Doas/
      Policy.lean
      Parser.lean
      Environment.lean
      Auth.lean
      Credentials.lean
      Sandbox.lean
      Main.lean
      Theorems.lean

  tools/
    import-openbsd-doas
    verify-upstream-hashes

  patches/
  UPSTREAM
  proof-plan.md
  flake.nix
  flake.lock
```

`upstream/` is immutable vendor material. Proof-discovered hardening changes go
under `patches/` (and, if worth upstreaming, become minimal OpenBSD-style diffs).

## Provenance workflow

The future import tool should conceptually perform:

```text
import-openbsd-doas <openbsd-src-commit>
        |
        +-- verify commit exists
        +-- extract usr.bin/doas only
        +-- write exact upstream commit to UPSTREAM
        +-- hash every imported file
        +-- reject local modifications to upstream/
        `-- regenerate proof source-binding metadata
```

A verification result must fail closed if the source hash changes.

## CompCert / compiled-artifact track

The source-level security theorem and the compiler theorem are separate layers.

Desired long-term chain:

```text
pinned OpenBSD doas source bytes
            |
            v
mechanically imported C / parser representation
            |
            v
Lean 4 memory + policy + privilege proofs
            |
            v
checked correspondence to CompCert C semantics
            |
            v
CompCert verified compilation
            |
            v
assembly
            |
            v
reproducibly assembled/linked artifact
```

Do not claim a machine-code theorem until the Lean-semantics ↔ CompCert-semantics
bridge is itself checked. Reproducible bits alone do not prove compiler
correctness.

## Suggested proof order

### D0 — threat model and source pin

- Pin exact OpenBSD source snapshot.
- Record file hashes.
- Enumerate all privileged state transitions and external calls.
- Define attacker-controlled inputs and trusted state.

Exit criterion: no ambiguous phrase such as "trusted environment" remains
without a concrete definition.

### D1 — pure policy semantics

Define a Lean model of:

- identities and groups;
- commands and arguments;
- rules;
- `permit` / `deny`;
- target users;
- command constraints;
- arguments constraints;
- `nopass`, `keepenv`, `setenv` and other policy-relevant options;
- exact rule-selection precedence.

Prove basic deterministic properties of rule selection independent of C.

Exit criterion: the intended authorization relation is executable/testable in
Lean and agrees with curated `doas.conf` examples.

### D2 — environment construction

Verify `env.c` memory safety and semantic correctness.

Exit criterion: the environment passed to exec is proved to be exactly the
policy-authorized transformation of the caller environment.

### D3 — policy matching in C

Connect the C rule representation and matching implementation to the Lean policy
semantics.

Exit criterion: C's selected rule refines `SelectedRule`.

### D4 — parser correspondence

Connect `parse.y` / generated parser output to the policy AST.

Exit criterion: accepted configurations produce the same rule sequence used by
the verified policy semantics; malformed configurations cannot smuggle an
unmodeled rule state into authorization.

### D5 — authentication state machine

Model the points at which authentication is required, performed, reused or
skipped.

Exit criterion: no exec path bypasses a required authentication obligation.

### D6 — credential transition

Model target user/group resolution and OpenBSD credential-setting contracts.

Exit criterion: every successful exec reaches the requested authorized target
credentials.

### D7 — config-file trust and sandbox

Verify configuration metadata checks and the `pledge` / `unveil` reduction
sequence.

Exit criterion: authorization does not consume an untrusted config and sandbox
capabilities never grow during execution.

### D8 — whole-program security theorem

Compose memory, parser, policy, auth, environment, credentials and sandbox
results.

Target statement, informally:

> Every execution of the pinned OpenBSD `doas` source that reaches command
> execution does so only through a parsed trusted configuration rule that
> permits that caller/target/command/argument tuple; all required authentication
> has succeeded; the target credentials and environment satisfy the selected
> rule; and the `doas` process itself has incurred no modeled memory undefined
> behavior.

### D9 — compiled artifact

Connect the source theorem through the CompCert bridge and reproducible build
pipeline if feasible.

Exit criterion: distinguish clearly between the kernel-checked source theorem
and any additional theorem/validation claim about generated machine artifacts.

## First actions while still inside `wall-c`

Do only the work that improves the reusable substrate:

1. finish a mechanically source-bound proof of `wall-c`'s
   `build_magic_packet`;
2. make memory and libc semantics independent of `wall-c` data structures;
3. establish a clean representation for external syscall contracts;
4. determine whether the chosen C importer can represent the C constructs used
   by OpenBSD `doas`;
5. separately investigate how `parse.y` output will enter the proof;
6. test CompCert against the language subset used by `doas` without yet claiming
   a compiler bridge;
7. only then create `doas-verify` and import the untouched OpenBSD snapshot.

## Migration trigger

Move this effort out of `wall-c` as soon as **both** are true:

1. one actual `wall-c` C function is mechanically imported and proved memory
   safe from source rather than from a handwritten Lean rewrite; and
2. the generic proof code has a stable boundary between C semantics and
   application-specific predicates.

This avoids both extremes: creating a speculative empty `doas` repository too
early, or letting `wall-c` become an unrelated monorepo for privilege-escalation
research.

## Success criterion

The eventual project succeeds only if the proof says something materially
stronger than "we modeled a program similar to doas."

The desired endpoint is an auditable chain from **exact OpenBSD source bytes** to
kernel-checked theorems about authorization, authentication obligations,
credential transition, environment confinement and memory safety, with every
external OpenBSD assumption named explicitly.