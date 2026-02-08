#include "wall.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Runtime overview:
 * 1) Read optional config MAC.
 * 2) Parse CLI arguments.
 * 3) Normalize and validate inputs.
 * 4) Optionally prompt for confirmation.
 * 5) Build and send WoL magic packet.
 */

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
