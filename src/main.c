#include "wall.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Runtime overview:
 * 1) Parse CLI arguments.
 * 2) Resolve target MACs by precedence:
 *    - explicit -m
 *    - one MAC from stdin
 *    - list from config file
 * 3) Normalize and validate inputs.
 * 4) Optionally prompt for confirmation.
 * 5) Build and send WoL magic packet(s).
 */

static int process_target(const char *raw_mac, const char *broadcast_ip, int port,
                          int prompt_enabled, int *had_error,
                          int *sent_count) {
    char normalized_mac[18];
    unsigned char mac_bin[MAC_ADDR_LEN];
    unsigned char magic_packet[PACKET_LEN];

    if (!normalize_mac(raw_mac, normalized_mac, sizeof(normalized_mac))) {
        fprintf(stderr,
                "Invalid MAC address format '%s'. Use XX:XX:XX:XX:XX:XX, "
                "XX-XX-XX-XX-XX-XX, or XXXXXXXXXXXX\n",
                raw_mac);
        *had_error = 1;
        return 0;
    }

    if (!validate_mac(normalized_mac)) {
        fprintf(stderr, "Invalid normalized MAC address format '%s'\n",
                normalized_mac);
        *had_error = 1;
        return 0;
    }

    if (prompt_enabled && !confirm_send(normalized_mac, broadcast_ip, port)) {
        printf("Cancelled for %s.\n", normalized_mac);
        return 0;
    }

    if (!parse_mac(normalized_mac, mac_bin)) {
        fprintf(stderr, "Failed to parse normalized MAC address '%s'\n",
                normalized_mac);
        *had_error = 1;
        return 0;
    }
    build_magic_packet(mac_bin, magic_packet);

    if (send_wol_packet(broadcast_ip, port, magic_packet, sizeof(magic_packet)) ==
        0) {
        printf("Magic packet sent to %s\n", normalized_mac);
        (*sent_count)++;
        return 1;
    }

    fprintf(stderr, "Failed to send magic packet to %s\n", normalized_mac);
    *had_error = 1;
    return 0;
}

#ifndef WALL_TEST
int main(int argc, char *argv[]) {
    const char *cli_mac = NULL;
    const char *broadcast_ip = "255.255.255.255";
    int port = DEFAULT_PORT;
    int skip_confirm = 0;
    int opt;
    int exit_code = EXIT_FAILURE;
    int stdin_is_tty = isatty(STDIN_FILENO);
    int had_error = 0;
    int sent_count = 0;
    char stdin_mac[MAX_MAC_INPUT_LEN];
    mac_list_t config_list = {0};
    int config_count = 0;

    while ((opt = getopt(argc, argv, "m:b:p:yh")) != -1) {
        switch (opt) {
        case 'm':
            cli_mac = optarg;
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

    if (!cli_mac && !stdin_is_tty) {
        int stdin_result = read_mac_from_stdin(stdin_mac, sizeof(stdin_mac));
        if (stdin_result < 0) {
            fprintf(stderr, "Failed to read MAC address from stdin\n");
            goto cleanup;
        }
        if (stdin_result == 1) {
            cli_mac = stdin_mac;
        }
    }

    if (cli_mac) {
        process_target(cli_mac, broadcast_ip, port,
                       should_prompt_for_confirmation(skip_confirm, stdin_is_tty),
                       &had_error, &sent_count);
    } else {
        config_count = read_macs_from_config(&config_list);
        if (config_count < 0) {
            fprintf(stderr, "Failed to read config file\n");
            goto cleanup;
        }
        if (config_count == 0) {
            fprintf(stderr,
                    "Error: No MAC address provided. Use -m, stdin, or configure "
                    "one or more entries in ~/.config/wall-c/config\n");
            goto cleanup;
        }

        for (int i = 0; i < config_count; i++) {
            process_target(config_list.items[i], broadcast_ip, port,
                           should_prompt_for_confirmation(skip_confirm, stdin_is_tty),
                           &had_error, &sent_count);
        }
    }

    if (had_error) {
        exit_code = EXIT_FAILURE;
    } else {
        exit_code = EXIT_SUCCESS;
    }

cleanup:
    free_mac_list(&config_list);
    return exit_code;
}
#endif
