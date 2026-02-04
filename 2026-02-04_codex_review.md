This is a fun little C tool to review — thanks for sharing it, and I’m happy to go deep. Below is an extensive, code‑review‑style assessment with findings first, then architecture/quality/testing, and finally recommended next steps.

**Findings (ordered by severity)**
1. **Buffer overflow risk when building config path (high).**
   In `read_mac_from_config`, the allocation for the `$HOME` fallback is too small. You allocate `strlen(home) + strlen(CONFIG_FILE_NAME) + 11`, but the format string uses `"%s/.config/%s"`. The literal `"/.config/"` is 9 chars, plus the null terminator, so you need `+ 10` not `+ 1`, and the current `+ 11` is still short by about 12 bytes once the null is included. This is a classic overflow for longer `$HOME` values. See `main.c:38-41`.
   File reference: `main.c:38`

2. **Macro redefinition likely breaks the build with `-Werror` (high).**
   `DEFAULT_PORT`, `MAC_ADDR_LEN`, and `PACKET_LEN` are defined in `wall.h` and then redefined in `main.c`. With `-Wall -Werror`, compilers can treat this as a hard error. This is an avoidable build hazard. See `wall.h:12-14` and `main.c:9-11`.
   File references: `wall.h:12`, `main.c:9`

3. **Memory leak when CLI `-m` is used (medium).**
   If `read_mac_from_config()` returns a heap string but the user supplies `-m`, the program never frees `config_mac` because it only frees when `!mac_from_cmdline`. That’s small, but consistent and easy to fix. See `main.c:202-214` and any return path like `main.c:280-291`.
   File references: `main.c:202`, `main.c:280`

4. **Port parsing uses `atoi` with no error detection (medium).**
   `atoi` treats non‑numeric input as `0`, which then fails validation, but there is no clear error about malformed input versus a literal zero. Also `atoi` doesn’t detect overflow. Use `strtol` with end‑ptr and range checks. See `main.c:219-220`.
   File reference: `main.c:219`

5. **Tests are compiled into production binary (low).**
   `main.c` includes `test.c` directly, so the runtime binary always contains test code. That’s fine for a tiny tool, but it makes production builds bigger and mixes test concerns into runtime. It also makes it harder to add a separate test runner. See `main.c:2`.
   File reference: `main.c:2`

**Architecture Overview**
The codebase is small and straightforward, and the core flow is clear:

1. Read MAC address from config (XDG path) or CLI.
2. Validate MAC, IP, port.
3. Optionally prompt for confirmation.
4. Build WoL magic packet.
5. Send UDP broadcast.

Most logic lives in `main.c`, with prototypes in `wall.h`. `test.c` is included into the build and uses `assert`‑based tests triggered by `-t`. That matches the README, and for a small utility it’s an entirely reasonable structure.

**Strengths**
- **Clear user flow and CLI ergonomics.** The flags are minimal, and the confirmation prompt is a good safety guard for a broadcast action.
- **Solid validation coverage.** MAC, IP, and port validation are in dedicated functions, which makes them testable.
- **XDG config support.** That’s a nice touch and makes the tool friendly for daily use.
- **Portable networking choices.** Using POSIX sockets and UDP broadcast is correct for WoL.
- **Tests for config edge cases.** The test suite checks whitespace trimming and “malicious” input patterns, which is good defensive thinking.

**Code Quality & Maintainability**
- **Centralization of constants is inconsistent.** `DEFAULT_PORT`, `MAC_ADDR_LEN`, and `PACKET_LEN` belong in one place only (`wall.h`), but are duplicated in `main.c`. This will drift over time or break builds.
- **`read_mac_from_config` is a bit long and does too much.** It handles path assembly, file IO, and trimming. Still fine, but you could simplify by extracting a small helper to build the config path or using `snprintf` into a fixed buffer.
- **Config path building is fragile.** The bug above is serious. This is an easy target for cleanup using `snprintf` with a single `size_t` calculation.
- **Tests are included as a source file.** It’s a clever shortcut, but it limits future growth. A separate test binary would improve hygiene.

**Security/Robustness Notes**
- **Buffer overflow risk in config path construction** is the largest risk and needs fixing.
- **Input validation is good** for MAC and IP but port parsing could be more robust as mentioned.
- **MAC parsing assumes validation already happened.** That’s OK given the flow, but if you ever reuse `parse_mac` independently, it should validate or at least check `sscanf` return values.
- **No privilege requirements.** Broadcast sockets are typically allowed, but some systems or firewall rules may block. The error handling is reasonable.

**Testing Coverage & Gaps**
What you cover well:
- MAC, IP, and port validation.
- Config reading, whitespace trimming, missing file behavior.
- Basic adversarial input patterns.

Gaps to consider:
- Parsing of command‑line args (e.g., invalid `-p` input like `-p abc`).
- Real packet generation correctness (e.g., a test that compares the packet bytes).
- Runtime errors (socket creation failure, `setsockopt` failure, invalid broadcast IP, etc.).
- No tests around `confirm_send`, which is likely fine but is untested.

**Build & Tooling**
- Makefile is clean and readable.
- `memcheck` is a nice touch for macOS/Linux.
- You’re building everything into one binary, which is okay for small tooling but starts to pinch if you want larger tests or a library extract.

**Recommended Next Steps (highest leverage)**
1. Fix the config path allocation bug in `read_mac_from_config` (`main.c:38-41`).
2. Remove duplicate macro definitions in `main.c` (`main.c:9-11`) and keep constants only in `wall.h`.
3. Replace `atoi` with `strtol` and explicit range + parse checks (`main.c:219`).
4. Free `config_mac` unconditionally before exit if it was allocated, even when `-m` is used (`main.c:202`, `main.c:280`).
5. Optional: split tests into a separate test target instead of `#include "test.c"`.

If you want, I can make these changes for you (very small patch) and keep the behavior the same.
Suggested next steps:
1. I can draft a minimal fix patch for the buffer overflow, macro cleanup, and `strtol` parsing.
2. I can also refactor tests into a separate compilation unit and add a small packet‑format test.
