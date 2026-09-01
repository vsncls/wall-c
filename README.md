# Wake-on-LAN Client (`wall-c`)

A small cross-platform C99 command-line tool for sending Wake-on-LAN magic packets.

`wall-c` deliberately has no configuration-file format. A target is supplied explicitly with `-m/--mac` or as one line on stdin. This keeps the runtime and verification surface small: no config discovery, parser, target database, aliases, or config-owned heap objects.

## Features

- MAC input from `-m` / `--mac` or stdin
- Broadcast IPv4 override or interface-derived broadcast
- Repeat controls with `--count` and `--interval-ms`
- `--dry-run`, `--quiet`, and `-y` for scripts
- Optional best-effort `--smart` wake probing
- GCC/Clang tests, ASan/UBSan, Linux/macOS CI
- zsh and fish completions

## Building

```sh
make
```

This creates `build/wall-c`.

Useful targets:

```sh
make test
make test-integration
make test-all
make sanitize
make release
make memcheck
```

Equivalent Zig targets include `zig build test`, `zig build sanitize`, and `zig build release`.

## Usage

```text
wall-c [-m MAC] [-b BROADCAST] [--interface IFACE] [-p PORT]
       [--count N] [--interval-ms MS] [--dry-run] [--quiet]
       [--smart] [-y] [-h] [--version]
```

Options:

- `-m`, `--mac MAC` — target MAC; accepts colon, hyphen, or compact form
- `-b`, `--broadcast IP` — broadcast IPv4, default `255.255.255.255`
- `--interface IFACE` — derive the broadcast IPv4 from an interface; conflicts with `-b`
- `-p`, `--port PORT` — UDP destination port, default `9`
- `--count N` — number of packets to send, default `1`
- `--interval-ms MS` — delay between repeated sends, default `0`
- `--dry-run` — validate and show the action without sending
- `--quiet` — reduce normal output
- `--smart` — best-effort pre/post wake probing
- `-y`, `--yes` — skip confirmation
- `-h`, `--help` — show help
- `--version` — show version

Target source precedence is intentionally simple:

1. `-m` / `--mac`
2. one MAC line from stdin

Non-interactive use requires `-y` because an interactive confirmation cannot be completed on piped stdin.

## Examples

```sh
wall-c -m AA:BB:CC:DD:EE:FF -y
wall-c --mac aabbccddeeff --dry-run -y
wall-c --mac AA:BB:CC:DD:EE:FF --interface en0 -y
printf 'AA:BB:CC:DD:EE:FF\n' | wall-c --dry-run -y
```

## Install

Default prefix is `/usr/local`:

```sh
make install
```

Use `PREFIX=/opt/wall-c` or `DESTDIR=/tmp/pkgroot` for custom/staged installs. `make uninstall` removes installed files.

## Shell completions

- zsh: `completions/zsh/_wall-c`
- fish: `completions/fish/wall-c.fish`

`make install` installs both completion files under the selected prefix.

## Verification experiment

`proof-plan.md` describes the Lean 4 / CompCert proof-of-concept. The first target is the exact C implementation of `build_magic_packet`, bound to source bytes before expanding toward whole-program memory safety.

## License

MIT. See `LICENSE`.
