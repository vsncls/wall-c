# Wake-on-LAN Client (wall-c)

A lightweight, cross-platform command-line tool for sending Wake-on-LAN (WoL) magic packets to wake up computers on your local network.

## Features

- Simple command-line interface with flag-based options
- XDG Base Directory specification compliant configuration
- Cross-platform support (macOS, Linux, and other Unix-like systems)
- Comprehensive input validation
- Built-in unit tests
- Memory leak detection support

## Building

```bash
make
```

This creates the executable at `build/wall-c`.

### Build Targets

- `make` or `make all` - Build the executable
- `make clean` - Remove build directory
- `make test` - Build and run unit tests
- `make test-integration` - Run CLI integration tests (exit codes / invalid args)
- `make test-all` - Run unit + integration tests
- `make sanitize` - Build and run tests with AddressSanitizer + UndefinedBehaviorSanitizer
- `make memcheck` - Build and run with memory leak detection (uses `leaks` on macOS, `valgrind` on Linux)

## Usage

```bash
./build/wall-c [-m <mac_address>] [-b <broadcast_ip>] [-p <port>] [-y] [-h]
```

### Options

- `-m <mac_address>` - Target MAC address (formats: `XX:XX:XX:XX:XX:XX`, `XX-XX-XX-XX-XX-XX`, or `XXXXXXXXXXXX`)
- `-b <broadcast_ip>` - Broadcast IP address (default: `255.255.255.255`)
- `-p <port>` - Port number (default: `9`)
- `-y` - Skip confirmation prompt
- `-h` - Display help message

By default, the program will display the packet details and ask for confirmation before sending. Use `-y` to bypass this prompt (required for non-interactive scripting/CI).

### Examples

Send a WoL packet with command-line arguments:
```bash
./build/wall-c -m AA:BB:CC:DD:EE:FF -b 192.168.1.255 -p 9
```

Use default broadcast IP and port:
```bash
./build/wall-c -m AA:BB:CC:DD:EE:FF
```

Run validation tests:
```bash
make test
# or
./build/wall-c-test
```

## Configuration File

Instead of specifying the MAC address on the command line, you can store it in a configuration file following the XDG Base Directory specification.

### Location

The configuration file is read from:
1. `$XDG_CONFIG_HOME/wall-c/config` (if `XDG_CONFIG_HOME` is set)
2. `~/.config/wall-c/config` (standard fallback)

### Setup

```bash
mkdir -p ~/.config/wall-c
echo "AA:BB:CC:DD:EE:FF" > ~/.config/wall-c/config
```

Then simply run:
```bash
./build/wall-c
```

The `-m` flag will override the MAC address from the config file if both are provided.

## Technical Details

- **Language**: C99
- **Networking**: POSIX socket API (UDP broadcast)
- **Magic Packet Format**: 6 bytes of 0xFF followed by the target MAC address repeated 16 times (102 bytes total)
- **Dependencies**: Standard C library and POSIX APIs only

## Testing

The project includes comprehensive unit tests covering:
- MAC address validation (valid/invalid formats)
- MAC normalization and parsing behavior
- IP address validation
- Port number validation
- Magic packet byte layout
- Configuration file reading with various edge cases

CLI integration tests cover:
- Exit codes for invalid argument combinations
- Non-interactive confirmation behavior

Run tests with memory leak detection:
```bash
make memcheck
```

This uses the native `leaks` utility on macOS or `valgrind` on Linux.

## License

See repository for license information.
