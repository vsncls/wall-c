#include "wall.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Runtime overview:
 * 1) Read optional config MAC.
 * 2) Parse CLI arguments.
 * 3) Normalize and validate inputs.
 * 4) Optionally prompt for confirmation.
 * 5) Build and send WoL magic packet.
 */

#define CONFIG_FILE_NAME "wall-c/config"
#define MAX_MAC_LEN 128

/* Internal helpers used only by this translation unit. */
#ifndef WALL_TEST
static void print_usage(const char *prog_name);
static int confirm_send(const char *mac, const char *broadcast_ip, int port);
#endif

/*
 * Read a MAC address from the XDG config location.
 *
 * Resolution order:
 * - $XDG_CONFIG_HOME/wall-c/config
 * - $HOME/.config/wall-c/config
 *
 * Returns a heap-allocated trimmed string or NULL when unavailable.
 */
char *read_mac_from_config(void) {
    char *config_path = NULL;
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    size_t path_size = 0;
    FILE *file = NULL;
    char *mac = NULL;

    if (xdg_config_home && xdg_config_home[0] != '\0') {
        path_size = strlen(xdg_config_home) + strlen(CONFIG_FILE_NAME) + 2;
        config_path = malloc(path_size);
        if (!config_path) {
            return NULL;
        }
        if (snprintf(config_path, path_size, "%s/%s", xdg_config_home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return NULL;
        }
    } else if (home && home[0] != '\0') {
        /* "/.config/" is 9 chars, plus null terminator. */
        path_size = strlen(home) + strlen(CONFIG_FILE_NAME) + 10;
        config_path = malloc(path_size);
        if (!config_path) {
            return NULL;
        }
        if (snprintf(config_path, path_size, "%s/.config/%s", home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return NULL;
        }
    } else {
        return NULL;
    }

    file = fopen(config_path, "r");
    free(config_path);
    if (!file) {
        return NULL;
    }

    mac = malloc(MAX_MAC_LEN);
    if (!mac) {
        fclose(file);
        return NULL;
    }

    if (fgets(mac, MAX_MAC_LEN, file)) {
        size_t len = strlen(mac);

        /* Trim trailing whitespace/newline characters. */
        while (len > 0 && (mac[len - 1] == '\n' || mac[len - 1] == '\r' ||
                           mac[len - 1] == ' ' || mac[len - 1] == '\t')) {
            mac[--len] = '\0';
        }

        /* Trim leading spaces/tabs by shifting the string in place. */
        char *start = mac;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (start != mac) {
            memmove(mac, start, strlen(start) + 1);
        }

        fclose(file);
        return mac;
    }

    free(mac);
    fclose(file);
    return NULL;
}

/* Print help text for CLI arguments. */
#ifndef WALL_TEST
static void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s [-m <mac_address>] [-b <broadcast_ip>] [-p <port>] "
            "[-y] [-h]\n",
            prog_name);
    fprintf(stderr, "  -m <mac_address>    : MAC address "
                    "(XX:XX:XX:XX:XX:XX, XX-XX-XX-XX-XX-XX, or XXXXXXXXXXXX)\n");
    fprintf(stderr,
            "  -b <broadcast_ip>   : Broadcast IP address "
            "(default: 255.255.255.255)\n");
    fprintf(stderr, "  -p <port>           : Port number (default: 9)\n");
    fprintf(stderr, "  -y                  : Skip confirmation prompt\n");
    fprintf(stderr, "  -h                  : Display this help message\n");
    fprintf(stderr, "\nConfig file location: $XDG_CONFIG_HOME/wall-c/config "
                    "or ~/.config/wall-c/config\n");
}
#endif

/* Interactive send confirmation. Returns 1 for yes, 0 otherwise. */
#ifndef WALL_TEST
static int confirm_send(const char *mac, const char *broadcast_ip, int port) {
    int c;

    printf("Send WoL packet?\n");
    printf("  MAC:       %s\n", mac);
    printf("  Broadcast: %s\n", broadcast_ip);
    printf("  Port:      %d\n", port);
    printf("Confirm [y/N]: ");
    fflush(stdout);

    c = getchar();
    while (getchar() != '\n' && !feof(stdin)) {
        ;
    }

    return (c == 'y' || c == 'Y');
}
#endif

/* Canonical validator used after normalization. */
int validate_mac(const char *mac) {
    if (strlen(mac) != 17) {
        return 0;
    }

    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':') {
                return 0;
            }
        } else if (!isxdigit((unsigned char)mac[i])) {
            return 0;
        }
    }

    return 1;
}

/*
 * Normalize supported MAC formats into canonical uppercase form.
 * Result: "AA:BB:CC:DD:EE:FF"
 */
int normalize_mac(const char *input_mac, char *normalized_mac,
                  size_t normalized_size) {
    char hex[13];
    size_t hex_index = 0;
    size_t len;

    if (!input_mac || !normalized_mac || normalized_size < 18) {
        return 0;
    }

    len = strlen(input_mac);
    if (len == 17) {
        char sep = input_mac[2];

        if (sep != ':' && sep != '-') {
            return 0;
        }

        for (size_t i = 0; i < len; i++) {
            if (i % 3 == 2) {
                if (input_mac[i] != sep) {
                    return 0;
                }
            } else {
                if (!isxdigit((unsigned char)input_mac[i])) {
                    return 0;
                }
                hex[hex_index++] =
                    (char)toupper((unsigned char)input_mac[i]);
            }
        }
    } else if (len == 12) {
        for (size_t i = 0; i < len; i++) {
            if (!isxdigit((unsigned char)input_mac[i])) {
                return 0;
            }
            hex[hex_index++] = (char)toupper((unsigned char)input_mac[i]);
        }
    } else {
        return 0;
    }

    if (hex_index != 12) {
        return 0;
    }
    hex[hex_index] = '\0';

    if (snprintf(normalized_mac, normalized_size,
                 "%.2s:%.2s:%.2s:%.2s:%.2s:%.2s", hex, hex + 2, hex + 4,
                 hex + 6, hex + 8, hex + 10) < 0) {
        return 0;
    }

    return 1;
}

int validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

int validate_port(int port) { return port > 0 && port <= 65535; }

int should_prompt_for_confirmation(int skip_confirm, int stdin_is_tty) {
    return !skip_confirm && stdin_is_tty;
}

/* Parse normalized MAC text into 6 bytes. */
int parse_mac(const char *mac_str, unsigned char *mac_bin) {
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        unsigned int byte = 0;
        if (sscanf(mac_str + (i * 3), "%2x", &byte) != 1) {
            return 0;
        }
        mac_bin[i] = (unsigned char)byte;
    }
    return 1;
}

void build_magic_packet(const unsigned char *mac, unsigned char *packet) {
    memset(packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(packet + 6 + (i * MAC_ADDR_LEN), mac, MAC_ADDR_LEN);
    }
}

int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len) {
    int sock;
    int broadcast_enable = 1;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                   sizeof(broadcast_enable)) < 0) {
        perror("Failed to enable broadcast");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, broadcast_ip, &addr.sin_addr) <= 0) {
        perror("Invalid broadcast IP");
        close(sock);
        return -1;
    }

    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&addr,
               sizeof(addr)) < 0) {
        perror("Failed to send packet");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

#ifndef WALL_TEST
int main(int argc, char *argv[]) {
    char *config_mac = read_mac_from_config();
    const char *mac_str = config_mac;
    const char *broadcast_ip = "255.255.255.255";
    int port = DEFAULT_PORT;
    int skip_confirm = 0;
    int exit_code = EXIT_FAILURE;
    int opt;
    int stdin_is_tty = isatty(STDIN_FILENO);
    char normalized_mac[18];
    unsigned char mac_bin[MAC_ADDR_LEN];
    unsigned char magic_packet[PACKET_LEN];

    while ((opt = getopt(argc, argv, "m:b:p:yh")) != -1) {
        switch (opt) {
        case 'm':
            mac_str = optarg;
            break;
        case 'b':
            broadcast_ip = optarg;
            break;
        case 'p': {
            char *endptr = NULL;
            long parsed_port;
            errno = 0;
            parsed_port = strtol(optarg, &endptr, 10);
            if (errno != 0 || endptr == optarg || *endptr != '\0' ||
                parsed_port < 1 || parsed_port > 65535) {
                fprintf(stderr,
                        "Invalid port value '%s'. Use a port between 1 and 65535\n",
                        optarg);
                goto cleanup;
            }
            port = (int)parsed_port;
            break;
        }
        case 'y':
            skip_confirm = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            exit_code = EXIT_SUCCESS;
            goto cleanup;
        default:
            print_usage(argv[0]);
            goto cleanup;
        }
    }

    if (!mac_str) {
        fprintf(stderr,
                "Error: No MAC address provided. Use -m option or configure in "
                "~/.config/wall-c/config\n");
        goto cleanup;
    }

    if (!normalize_mac(mac_str, normalized_mac, sizeof(normalized_mac))) {
        fprintf(stderr,
                "Invalid MAC address format. Use XX:XX:XX:XX:XX:XX, "
                "XX-XX-XX-XX-XX-XX, or XXXXXXXXXXXX\n");
        goto cleanup;
    }
    mac_str = normalized_mac;

    if (!validate_mac(mac_str)) {
        fprintf(stderr, "Invalid normalized MAC address format\n");
        goto cleanup;
    }

    if (!validate_ip(broadcast_ip)) {
        fprintf(stderr,
                "Invalid IP address format. Use IPv4 format (e.g., 192.168.1.255)\n");
        goto cleanup;
    }

    if (!validate_port(port)) {
        fprintf(stderr, "Invalid port number. Use a port between 1 and 65535\n");
        goto cleanup;
    }

    if (!skip_confirm && !stdin_is_tty) {
        fprintf(stderr,
                "Non-interactive stdin detected. Use -y to skip confirmation.\n");
        goto cleanup;
    }

    if (should_prompt_for_confirmation(skip_confirm, stdin_is_tty) &&
        !confirm_send(mac_str, broadcast_ip, port)) {
        printf("Cancelled.\n");
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }

    if (!parse_mac(mac_str, mac_bin)) {
        fprintf(stderr, "Failed to parse normalized MAC address\n");
        goto cleanup;
    }
    build_magic_packet(mac_bin, magic_packet);

    if (send_wol_packet(broadcast_ip, port, magic_packet,
                        sizeof(magic_packet)) == 0) {
        printf("Magic packet sent to %s\n", mac_str);
        exit_code = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Failed to send magic packet\n");
    }

cleanup:
    free(config_mac);
    return exit_code;
}
#endif
