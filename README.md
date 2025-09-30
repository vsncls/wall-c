# Wake-on-LAN Client

A simple command-line tool for sending Wake-on-LAN (WoL) magic packets to wake up a computer on the same local network.

## Usage

```
./wall-c [MAC_ADDRESS] [BROADCAST_IP] [PORT]
```

- `MAC_ADDRESS`: The MAC address of the target computer (e.g., `AA:BB:CC:DD:EE:FF`).
- `BROADCAST_IP`: The broadcast IP address for the network (e.g., `192.168.1.255`).
- `PORT`: The port number (usually `9`).

If no arguments are provided, the program will use hardcoded default values.
