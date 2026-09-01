## TODO (`wall-c`)

### Completed (Recent)

- [x] Keep the runtime dependency-light C99/POSIX.
- [x] Support explicit MAC input through `-m` / `--mac` and one-line stdin.
- [x] Support broadcast override, interface-derived broadcast, port, repeat count, and repeat interval.
- [x] Support `--dry-run`, `--quiet`, `--smart`, and non-interactive `-y` operation.
- [x] Add unit tests, CLI integration tests, ASan/UBSan CI, shell lint, and Linux/macOS compiler matrices.
- [x] Add Make and Zig build paths, install/uninstall, man page, and zsh/fish completions.
- [x] Remove configuration-file support, named-target parsing, config discovery, dynamic target lists, `--target`, `--list-targets`, and `--continue-on-error` to reduce runtime and verification attack surface.

### Formal Verification PoC

`wall-c` is the only verification target for this phase. See `proof-plan.md`.

- [ ] Generate and commit `flake.lock`; validate `nix flake check` on Nix-capable macOS/Linux hosts.
- [ ] Confirm pinned CompCert can compile the simplified `wall-c` subset used by the first proof target.
- [ ] Create the Lean 4 proof project.
- [ ] Mechanically import the exact C implementation of `build_magic_packet`.
- [ ] Bind the imported representation to exact source hashes.
- [ ] Prove memory safety and exact 102-byte packet layout.
- [ ] Record `#print axioms` and all explicit assumptions.
- [ ] Establish a checked Lean-semantics ↔ CompCert correspondence for the first function.
- [ ] Reproduce assembly/artifact hashes under locked Nix inputs.
- [ ] Only after the end-to-end PoC works, expand module-by-module toward whole-program `wall-c` memory safety.

### Next Candidates

Keep feature work conservative while verification is underway. Any new parser, persistence layer, dynamic target database, or process-execution surface needs a strong justification.

### Done Criteria

- [ ] CI green on supported Linux/macOS compiler targets after each change.
- [ ] Sanitizer jobs remain zero findings.
- [ ] Release workflow validated on the next `v*` tag.
