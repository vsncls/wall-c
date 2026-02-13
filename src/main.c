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
    /* strtol gives robust parsing and overflow detection for user input. */
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

static const wake_target_t *find_named_target(const target_list_t *list,
                                              const char *target_name) {
    if (!list || !target_name) {
        return NULL;
    }

    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].name &&
            strcmp(list->items[i].name, target_name) == 0) {
            return &list->items[i];
        }
    }

    return NULL;
}

static void print_target_list(const target_list_t *list) {
    if (!list || list->count == 0) {
        printf("No configured targets found.\n");
        return;
    }

    for (size_t i = 0; i < list->count; i++) {
        const char *name = list->items[i].name;
        if (name) {
            printf("%s\t%s\t%s\t%d\n", name, list->items[i].mac,
                   list->items[i].broadcast_ip, list->items[i].port);
        } else {
            printf("(unnamed-%zu)\t%s\t%s\t%d\n", i + 1, list->items[i].mac,
                   list->items[i].broadcast_ip, list->items[i].port);
        }
    }
}

#ifndef WALL_TEST
int main(int argc, char *argv[]) {
    const char *cli_mac = NULL;
    const char *target_name = NULL;
    const char *broadcast_ip = "255.255.255.255";
    const char *interface_name = NULL;
    char resolved_broadcast[INET_ADDRSTRLEN];
    int port = DEFAULT_PORT;
    int has_broadcast_override = 0;
    int has_port_override = 0;
    int list_targets = 0;
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
    int smart_mode = 0;
    int smart_timeout_ms = 1500;
    int smart_probe_interval_ms = 250;
    wake_send_options_t send_options = {0};
    char stdin_mac[MAX_MAC_INPUT_LEN];
    target_list_t config_list = {0};
    int stdin_has_mac = 0;
    int config_count = 0;
    const struct option long_options[] = {
        {"mac", required_argument, NULL, 'm'},
        {"broadcast", required_argument, NULL, 'b'},
        {"port", required_argument, NULL, 'p'},
        {"yes", no_argument, NULL, 'y'},
        {"help", no_argument, NULL, 'h'},
        {"target", required_argument, NULL, 1006},
        {"list-targets", no_argument, NULL, 1007},
        {"interface", required_argument, NULL, 1008},
        {"version", no_argument, NULL, 1000},
        {"dry-run", no_argument, NULL, 1001},
        {"quiet", no_argument, NULL, 1002},
        {"count", required_argument, NULL, 1003},
        {"interval-ms", required_argument, NULL, 1004},
        {"continue-on-error", no_argument, NULL, 1005},
        {"smart", no_argument, NULL, 1009},
        {0, 0, 0, 0}};

    /* Parse short and long CLI options into runtime settings. */
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
                goto cleanup;
            }
            has_port_override = 1;
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
        case 1006:
            target_name = optarg;
            break;
        case 1007:
            list_targets = 1;
            break;
        case 1008:
            interface_name = optarg;
            break;
        case 1009:
            smart_mode = 1;
            break;
        default:
            print_usage(argv[0]);
            goto cleanup;
        }
    }

    if (list_targets) {
        config_count = read_targets_from_config(&config_list);
        if (config_count < 0) {
            fprintf(stderr, "Failed to parse config file\n");
            goto cleanup;
        }
        print_target_list(&config_list);
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }

    if (interface_name && has_broadcast_override) {
        fprintf(stderr,
                "Options --interface and -b/--broadcast cannot be combined\n");
        goto cleanup;
    }
    if (interface_name) {
        int resolved =
            resolve_interface_broadcast(interface_name, resolved_broadcast,
                                        sizeof(resolved_broadcast));
        if (resolved < 0) {
            fprintf(stderr, "Failed to resolve interface '%s'\n", interface_name);
            goto cleanup;
        }
        if (resolved == 0) {
            fprintf(stderr,
                    "No IPv4 broadcast address found for interface '%s'\n",
                    interface_name);
            goto cleanup;
        }
        broadcast_ip = resolved_broadcast;
        has_broadcast_override = 1;
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

    if (!cli_mac && !target_name && !stdin_is_tty) {
        /* In pipelines, allow a MAC from stdin unless -m/--target is provided. */
        int stdin_result = read_mac_from_stdin(stdin_mac, sizeof(stdin_mac));
        if (stdin_result < 0) {
            fprintf(stderr, "Failed to read MAC address from stdin\n");
            goto cleanup;
        }
        if (stdin_result == 1) {
            stdin_has_mac = 1;
        }
    }

    if (cli_mac) {
        /* Highest precedence: explicit CLI MAC. */
        send_options = build_send_options(
            skip_confirm, stdin_is_tty, repeat_count, interval_ms, dry_run,
            quiet, smart_mode, smart_timeout_ms, smart_probe_interval_ms);
        engine_process_target(cli_mac, broadcast_ip, port, &send_options,
                              &had_error, &sent_count);
    } else if (stdin_has_mac) {
        /* Next precedence: MAC from stdin. */
        send_options = build_send_options(
            skip_confirm, stdin_is_tty, repeat_count, interval_ms, dry_run,
            quiet, smart_mode, smart_timeout_ms, smart_probe_interval_ms);
        engine_process_target(stdin_mac, broadcast_ip, port, &send_options,
                              &had_error, &sent_count);
    } else {
        /* Final fallback: config file (single named target or all targets). */
        config_count = read_targets_from_config(&config_list);
        if (config_count < 0) {
            fprintf(stderr, "Failed to parse config file\n");
            goto cleanup;
        }
        if (config_count == 0) {
            fprintf(stderr,
                    "Error: No MAC address provided. Use -m, stdin, or configure "
                    "one or more entries in ~/.config/wall-c/config\n");
            goto cleanup;
        }

        if (target_name) {
            const wake_target_t *target =
                find_named_target(&config_list, target_name);
            if (!target) {
                fprintf(stderr, "Target '%s' not found in config file\n",
                        target_name);
                goto cleanup;
            }

            send_options = build_send_options(
                skip_confirm, stdin_is_tty, repeat_count, interval_ms, dry_run,
                quiet, smart_mode, smart_timeout_ms, smart_probe_interval_ms);
            engine_process_target(
                target->mac,
                has_broadcast_override ? broadcast_ip : target->broadcast_ip,
                has_port_override ? port : target->port, &send_options,
                &had_error, &sent_count);
        } else {
            send_options = build_send_options(
                skip_confirm, stdin_is_tty, repeat_count, interval_ms, dry_run,
                quiet, smart_mode, smart_timeout_ms, smart_probe_interval_ms);
            for (int i = 0; i < config_count; i++) {
                int ok = engine_process_target(
                    config_list.items[i].mac,
                    has_broadcast_override ? broadcast_ip
                                           : config_list.items[i].broadcast_ip,
                    has_port_override ? port : config_list.items[i].port,
                    &send_options, &had_error, &sent_count);
                if (!ok && !continue_on_error) {
                    break;
                }
            }
        }
    }

    if (had_error) {
        exit_code = EXIT_FAILURE;
    } else {
        exit_code = EXIT_SUCCESS;
    }

cleanup:
    free_target_list(&config_list);
    return exit_code;
}
#endif
