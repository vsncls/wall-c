#include "wall.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Runtime overview:
 * 1) Parse CLI arguments.
 * 2) Take one target MAC from -m/--mac or stdin.
 * 3) Validate network inputs.
 * 4) Build and send the WoL packet, optionally with repeats/smart probing.
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

static wake_send_options_t
build_send_options(int skip_confirm, int stdin_is_tty, int repeat_count,
                   int interval_ms, int dry_run, int quiet, int smart_mode,
                   int smart_timeout_ms, int smart_probe_interval_ms) {
    wake_send_options_t options = {0};

    options.prompt_enabled =
        should_prompt_for_confirmation(skip_confirm, stdin_is_tty);
    options.repeat_count = repeat_count;
    options.interval_ms = interval_ms;
    options.dry_run = dry_run;
    options.quiet = quiet;
    options.smart_mode = smart_mode;
    options.smart_timeout_ms = smart_timeout_ms;
    options.smart_probe_interval_ms = smart_probe_interval_ms;
    return options;
}

#ifndef WALL_TEST
int main(int argc, char *argv[]) {
    const char *cli_mac = NULL;
    const char *broadcast_ip = "255.255.255.255";
    const char *interface_name = NULL;
    const char *target_mac = NULL;
    char resolved_broadcast[INET_ADDRSTRLEN];
    char stdin_mac[MAX_MAC_INPUT_LEN];
    int port = DEFAULT_PORT;
    int has_broadcast_override = 0;
    int skip_confirm = 0;
    int opt;
    int stdin_is_tty = isatty(STDIN_FILENO);
    int had_error = 0;
    int sent_count = 0;
    int dry_run = 0;
    int quiet = 0;
    int repeat_count = 1;
    int interval_ms = 0;
    int smart_mode = 0;
    int smart_timeout_ms = 1500;
    int smart_probe_interval_ms = 250;
    wake_send_options_t send_options = {0};
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
        {"interface", required_argument, NULL, 1005},
        {"smart", no_argument, NULL, 1006},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "m:b:p:yh", long_options, NULL)) !=
           -1) {
        switch (opt) {
        case 'm':
            cli_mac = optarg;
            break;
        case 'b':
            broadcast_ip = optarg;
            has_broadcast_override = 1;
            break;
        case 'p':
            if (!parse_bounded_int(optarg, 1, 65535, "port", &port)) {
                return EXIT_FAILURE;
            }
            break;
        case 'y':
            skip_confirm = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 1000:
            print_version();
            return EXIT_SUCCESS;
        case 1001:
            dry_run = 1;
            break;
        case 1002:
            quiet = 1;
            break;
        case 1003:
            if (!parse_bounded_int(optarg, 1, INT_MAX, "count", &repeat_count)) {
                return EXIT_FAILURE;
            }
            break;
        case 1004:
            if (!parse_bounded_int(optarg, 0, INT_MAX, "interval-ms",
                                   &interval_ms)) {
                return EXIT_FAILURE;
            }
            break;
        case 1005:
            interface_name = optarg;
            break;
        case 1006:
            smart_mode = 1;
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (interface_name && has_broadcast_override) {
        fprintf(stderr,
                "Options --interface and -b/--broadcast cannot be combined\n");
        return EXIT_FAILURE;
    }

    if (interface_name) {
        int resolved =
            resolve_interface_broadcast(interface_name, resolved_broadcast,
                                        sizeof(resolved_broadcast));
        if (resolved < 0) {
            fprintf(stderr, "Failed to resolve interface '%s'\n", interface_name);
            return EXIT_FAILURE;
        }
        if (resolved == 0) {
            fprintf(stderr,
                    "No IPv4 broadcast address found for interface '%s'\n",
                    interface_name);
            return EXIT_FAILURE;
        }
        broadcast_ip = resolved_broadcast;
    }

    if (!validate_ip(broadcast_ip)) {
        fprintf(stderr,
                "Invalid IP address format. Use IPv4 format (e.g., 192.168.1.255)\n");
        return EXIT_FAILURE;
    }

    if (!validate_port(port)) {
        fprintf(stderr, "Invalid port number. Use a port between 1 and 65535\n");
        return EXIT_FAILURE;
    }

    if (!skip_confirm && !stdin_is_tty) {
        fprintf(stderr,
                "Non-interactive stdin detected. Use -y to skip confirmation.\n");
        return EXIT_FAILURE;
    }

    if (cli_mac) {
        target_mac = cli_mac;
    } else if (!stdin_is_tty) {
        int stdin_result = read_mac_from_stdin(stdin_mac, sizeof(stdin_mac));
        if (stdin_result < 0) {
            fprintf(stderr, "Failed to read MAC address from stdin\n");
            return EXIT_FAILURE;
        }
        if (stdin_result == 1) {
            target_mac = stdin_mac;
        }
    }

    if (!target_mac) {
        fprintf(stderr,
                "Error: No MAC address provided. Use -m/--mac or stdin.\n");
        return EXIT_FAILURE;
    }

    send_options = build_send_options(
        skip_confirm, stdin_is_tty, repeat_count, interval_ms, dry_run, quiet,
        smart_mode, smart_timeout_ms, smart_probe_interval_ms);
    engine_process_target(target_mac, broadcast_ip, port, &send_options,
                          &had_error, &sent_count);

    return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
