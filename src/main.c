#include "wall.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
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
 * 5) Build and send WoL magic packet(s), optionally with repeat delay.
 */

static int parse_bounded_int(const char *value, int min_value, int max_value,
                             const char *label, int *out_value) {
    char *endptr = NULL;
    long parsed;

    if (!value || !out_value) {
        return 0;
    }

    errno = 0;
    parsed = strtol(value, &endptr, 10);
    if (errno != 0 || endptr == value || *endptr != '\0' ||
        parsed < min_value || parsed > max_value) {
        fprintf(stderr, "Invalid %s value '%s'. Use a value between %d and %d\n",
                label, value, min_value, max_value);
        return 0;
    }

    *out_value = (int)parsed;
    return 1;
}

static int process_target(const char *raw_mac, const char *broadcast_ip, int port,
                          int prompt_enabled, int repeat_count, int interval_ms,
                          int dry_run, int quiet, int *had_error,
                          int *sent_count) {
    char normalized_mac[18];
    unsigned char mac_bin[MAC_ADDR_LEN];
    unsigned char magic_packet[PACKET_LEN];
    int attempt;

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
        if (!quiet) {
            printf("Cancelled for %s.\n", normalized_mac);
        }
        return 0;
    }

    for (attempt = 0; attempt < repeat_count; attempt++) {
        if (dry_run) {
            if (!quiet) {
                printf("DRY RUN: would send to %s via %s:%d (attempt %d/%d)\n",
                       normalized_mac, broadcast_ip, port, attempt + 1,
                       repeat_count);
            }
            (*sent_count)++;
        } else {
            if (!parse_mac(normalized_mac, mac_bin)) {
                fprintf(stderr, "Failed to parse normalized MAC address '%s'\n",
                        normalized_mac);
                *had_error = 1;
                return 0;
            }
            build_magic_packet(mac_bin, magic_packet);

            if (send_wol_packet(broadcast_ip, port, magic_packet,
                                sizeof(magic_packet)) != 0) {
                fprintf(stderr, "Failed to send magic packet to %s (attempt %d/%d)\n",
                        normalized_mac, attempt + 1, repeat_count);
                *had_error = 1;
                return 0;
            }

            if (!quiet) {
                printf("Magic packet sent to %s (attempt %d/%d)\n", normalized_mac,
                       attempt + 1, repeat_count);
            }
            (*sent_count)++;
        }

        if (attempt + 1 < repeat_count && interval_ms > 0) {
            usleep((useconds_t)interval_ms * 1000U);
        }
    }

    return 1;
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
    int dry_run = 0;
    int quiet = 0;
    int continue_on_error = 0;
    int repeat_count = 1;
    int interval_ms = 0;
    char stdin_mac[MAX_MAC_INPUT_LEN];
    mac_list_t config_list = {0};
    int config_count = 0;
    const struct option long_options[] = {
        {"mac", required_argument, NULL, 'm'},
        {"broadcast", required_argument, NULL, 'b'},
        {"port", required_argument, NULL, 'p'},
        {"yes", no_argument, NULL, 'y'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 1000},
        {"dry-run", no_argument, NULL, 1001},
        {"quiet", no_argument, NULL, 1002},
        {"count", required_argument, NULL, 1003},
        {"interval-ms", required_argument, NULL, 1004},
        {"continue-on-error", no_argument, NULL, 1005},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "m:b:p:yh", long_options, NULL)) !=
           -1) {
        switch (opt) {
        case 'm':
            cli_mac = optarg;
            break;
        case 'b':
            broadcast_ip = optarg;
            break;
        case 'p':
            if (!parse_bounded_int(optarg, 1, 65535, "port", &port)) {
                goto cleanup;
            }
            break;
        case 'y':
            skip_confirm = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            exit_code = EXIT_SUCCESS;
            goto cleanup;
        case 1000:
            print_version();
            exit_code = EXIT_SUCCESS;
            goto cleanup;
        case 1001:
            dry_run = 1;
            break;
        case 1002:
            quiet = 1;
            break;
        case 1003:
            if (!parse_bounded_int(optarg, 1, INT_MAX, "count", &repeat_count)) {
                goto cleanup;
            }
            break;
        case 1004:
            if (!parse_bounded_int(optarg, 0, INT_MAX, "interval-ms",
                                   &interval_ms)) {
                goto cleanup;
            }
            break;
        case 1005:
            continue_on_error = 1;
            break;
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
                       repeat_count, interval_ms, dry_run, quiet, &had_error,
                       &sent_count);
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
            int ok = process_target(
                config_list.items[i], broadcast_ip, port,
                should_prompt_for_confirmation(skip_confirm, stdin_is_tty),
                repeat_count, interval_ms, dry_run, quiet, &had_error,
                &sent_count);
            if (!ok && !continue_on_error) {
                break;
            }
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
