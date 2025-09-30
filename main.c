#include "wall.h"
#include "test.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For getopt

#define DEFAULT_PORT 9
#define MAC_ADDR_LEN 6
#define PACKET_LEN 102
#define HARDCODED_MAC "AA:BB:CC:DD:EE:FF" // Dewey hardcoded MAC address

// Function prototypes
void print_usage(const char *prog_name);
int validate_mac(const char *mac);
int validate_ip(const char *ip);
int validate_port(int port);
void parse_mac(const char *mac_str, unsigned char *mac_bin);
void build_magic_packet(const unsigned char *mac, unsigned char *packet);
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len);


// Print usage information
void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [-m <mac_address>] [-b <broadcast_ip>] [-p <port>] [-t]\n", prog_name);
    fprintf(stderr, "  -m <mac_address>    : MAC address to wake up (default: %s)\n", HARDCODED_MAC);
    fprintf(stderr, "  -b <broadcast_ip>   : Broadcast IP address (default: 255.255.255.255)\n");
    fprintf(stderr, "  -p <port>           : Port number (default: 9)\n");
    fprintf(stderr, "  -t                  : Run validation tests\n");
    fprintf(stderr, "  -h                  : Display this help message\n");
}

// Validate MAC address format (e.g., "AA:BB:CC:DD:EE:FF")
int validate_mac(const char *mac) {
    if (strlen(mac) != 17)
        return 0; // Format should be "XX:XX:XX:XX:XX:XX"
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':')
                return 0; // Ensure ':' separators
        } else {
            if (!isxdigit(mac[i]))
                return 0; // Ensure hexadecimal digits
        }
    }
    return 1;
}

// Validate IP address format (e.g., "192.168.1.255")
int validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

// Validate port number (1-65535)
int validate_port(int port) {
    return port > 0 && port <= 65535;
}

// Parse MAC address into binary format
void parse_mac(const char *mac_str, unsigned char *mac_bin) {
  for (int i = 0; i < MAC_ADDR_LEN; i++) {
    sscanf(mac_str + (i * 3), "%2hhx", &mac_bin[i]);
  }
}

// Build the magic packet
void build_magic_packet(const unsigned char *mac, unsigned char *packet) {
  memset(packet, 0xFF, 6); // First 6 bytes are 0xFF
  for (int i = 0; i < 16; i++) {
    memcpy(packet + 6 + (i * MAC_ADDR_LEN), mac, MAC_ADDR_LEN);
  }
}

// Send the magic packet via UDP
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len) {
  int sock;
  struct sockaddr_in addr;

  // Create a UDP socket
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Enable broadcast
  int broadcast_enable = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                 sizeof(broadcast_enable)) < 0) {
    perror("Failed to enable broadcast");
    close(sock);
    return -1;
  }

  // Configure destination address
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, broadcast_ip, &addr.sin_addr) <= 0) {
    perror("Invalid broadcast IP");
    close(sock);
    return -1;
  }

  // Send the magic packet
  if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
    perror("Failed to send packet");
    close(sock);
    return -1;
  }

  close(sock);
  return 0;
}

// Main function
int main(int argc, char *argv[]) {
    const char *mac_str = HARDCODED_MAC;
    const char *broadcast_ip = "255.255.255.255"; // Default broadcast IP
    int port = DEFAULT_PORT;
    int opt;

    while ((opt = getopt(argc, argv, "m:b:p:th")) != -1) {
        switch (opt) {
            case 'm':
                mac_str = optarg;
                break;
            case 'b':
                broadcast_ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 't':
                test_validation();
                return EXIT_SUCCESS;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default: /* '?' */
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (!validate_mac(mac_str)) {
        fprintf(stderr, "Invalid MAC address format. Use XX:XX:XX:XX:XX:XX\n");
        return EXIT_FAILURE;
    }

    if (!validate_ip(broadcast_ip)) {
        fprintf(stderr, "Invalid IP address format. Use IPv4 format (e.g., 192.168.1.255)\n");
        return EXIT_FAILURE;
    }

    if (!validate_port(port)) {
        fprintf(stderr, "Invalid port number. Use a port between 1 and 65535\n");
        return EXIT_FAILURE;
    }

    unsigned char mac_bin[MAC_ADDR_LEN];
    parse_mac(mac_str, mac_bin);

    unsigned char magic_packet[PACKET_LEN];
    build_magic_packet(mac_bin, magic_packet);

    if (send_wol_packet(broadcast_ip, port, magic_packet, sizeof(magic_packet)) == 0) {
        printf("Magic packet sent to %s\n", mac_str);
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Failed to send magic packet\n");
        return EXIT_FAILURE;
    }
}
