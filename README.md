# Wake-on-LAN Client (`wall-c`)

A lightweight, cross-platform command-line tool for sending Wake-on-LAN (WoL) magic packets to wake up hosts on your local network.

## Features

- Short and long CLI flags (`-m` / `--mac`, `-b` / `--broadcast`, ...)
- Multi-target config support with optional aliases
- Target selection by name (`--target`) and discovery (`--list-targets`)
- Batch send controls (`--count`, `--interval-ms`, `--continue-on-error`)
- Interface-aware broadcast resolution (`--interface <ifname>`)
- Script-friendly behavior (`--dry-run`, `--quiet`, `-y`)
- Unit tests, CLI integration tests, sanitizer target, CI coverage on macOS/Linux
- Built-in completion helpers for zsh and fish

## Building

```bash
make
```

This creates `build/wall-c`.

### Build Targets

- `make` or `make all` - Build `build/wall-c`
- `make clean` - Remove the build directory
- `make test` - Build and run unit tests
- `make test-integration` - Run CLI integration tests
- `make test-all` - Run unit + integration tests
- `make sanitize` - Run tests with ASan + UBSan
- `make release` - Build optimized `build/wall-c-release`
- `make memcheck` - Leak checks (`leaks` on macOS, `valgrind` on Linux)
- `make install` - Install binary, man page, and completions
- `make uninstall` - Remove installed files

Equivalent Zig targets:
- `zig build test`
- `zig build sanitize`
- `zig build release`

## Install

Default install prefix is `/usr/local`.

```bash
make install
```

Custom prefix:

```bash
make install PREFIX=/opt/wall-c
```

Package-style staged install:

```bash
make install DESTDIR=/tmp/pkgroot
```

Uninstall:

```bash
make uninstall
```

## Usage

```bash
./build/wall-c \
  [-m <mac_address>] [--target <name>] [--list-targets] \
  [-b <broadcast_ip>] [--interface <ifname>] [-p <port>] \
  [--count <n>] [--interval-ms <ms>] [--continue-on-error] \
  [--dry-run] [--quiet] [--smart] [-y] [-h] [--version]
```

### Options

- `-m`, `--mac <mac_address>` - Target MAC address
- `--target <name>` - Send only one named target from config
- `--list-targets` - Print parsed config targets and exit
- `-b`, `--broadcast <broadcast_ip>` - Broadcast IPv4 address (default: `255.255.255.255`)
- `--interface <ifname>` - Resolve broadcast IPv4 from interface (cannot be combined with `-b`)
- `-p`, `--port <port>` - UDP destination port (default: `9`)
- `--count <n>` - Send packet `n` times per target (default: `1`)
- `--interval-ms <ms>` - Delay between repeated sends (default: `0`)
- `--continue-on-error` - Continue processing later targets after a failure
- `--dry-run` - Validate and print actions without sending packets
- `--quiet` - Reduce non-error output
- `--smart` - Probe if host already appears awake before sending and verify after send (best effort)
- `-y`, `--yes` - Skip confirmation prompt
- `-h`, `--help` - Show help
- `--version` - Show version

Default behavior prompts for confirmation before sending. For non-interactive usage (CI/scripts), pass `-y`.

Target precedence:
1. `-m` / `--mac`
2. `--target`
3. first non-empty, non-comment line from `stdin`
4. all targets from config file

### Examples

Send one packet:

```bash
./build/wall-c -m AA:BB:CC:DD:EE:FF -y
```

Dry-run with repeats:

```bash
./build/wall-c --mac AA:BB:CC:DD:EE:FF --dry-run --count 3 --interval-ms 200 -y
```

Use interface-derived broadcast:

```bash
./build/wall-c --mac AA:BB:CC:DD:EE:FF --interface en0 -y
```

Send a named target from config:

```bash
./build/wall-c --target nas -y
```

Read one MAC from stdin:

```bash
echo "AA:BB:CC:DD:EE:FF" | ./build/wall-c -y
```

## Configuration

Config path resolution:
1. `$XDG_CONFIG_HOME/wall-c/config`
2. `$HOME/.config/wall-c/config`

Supported line formats:

```text
MAC [BROADCAST_IP] [PORT]
NAME MAC [BROADCAST_IP] [PORT]
```

Blank lines and `#` comments are ignored.

Example:

```bash
mkdir -p ~/.config/wall-c
cat > ~/.config/wall-c/config <<'EOF'
# unnamed target (uses defaults)
AA:BB:CC:DD:EE:11

# named targets
nas AA:BB:CC:DD:EE:FF 192.168.1.255 9
media 11:22:33:44:55:66
EOF
```

## Shell Completions

Included files:
- zsh: `completions/zsh/_wall-c`
- fish: `completions/fish/wall-c.fish`

`make install` places completion files into:
- `${PREFIX}/share/zsh/site-functions/_wall-c`
- `${PREFIX}/share/fish/vendor_completions.d/wall-c.fish`

Manual user-local install:

```bash
mkdir -p ~/.zsh/completions
cp completions/zsh/_wall-c ~/.zsh/completions/
```

```bash
mkdir -p ~/.config/fish/completions
cp completions/fish/wall-c.fish ~/.config/fish/completions/
```

## Testing

```bash
make test-all
make sanitize
make memcheck
```

CI runs:
- Ubuntu + macOS
- `gcc` + `clang` build/test matrix
- sanitizer jobs (clang) on Ubuntu + macOS
- shell lint (`shellcheck`) for integration script

## Release Artifacts

A release workflow builds tagged releases (`v*`) for Linux and macOS, then uploads:
- tarball artifacts containing binary + man page + completions
- corresponding `.sha256` checksum files

## Troubleshooting

- Host does not wake:
  Ensure BIOS/UEFI WoL is enabled and NIC power settings allow wake.
- Different subnet:
  WoL broadcasts often do not traverse routers by default. Use directed broadcast routing or run `wall-c` on the target LAN.
- Non-interactive failures:
  Use `-y` in scripts and CI.

## License

MIT. See `LICENSE`.
