#include "wall.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void sleep_ms(int interval_ms) {
    struct timespec req;
    struct timespec rem;

    if (interval_ms <= 0) {
        return;
    }

    req.tv_sec = interval_ms / 1000;
    req.tv_nsec = (long)(interval_ms % 1000) * 1000000L;

    while (nanosleep(&req, &rem) != 0 && errno == EINTR) {
        req = rem;
    }
}

int engine_process_target(const char *raw_mac, const char *broadcast_ip, int port,
                          const wake_send_options_t *options, int *had_error,
                          int *sent_count) {
    char normalized_mac[18];
    unsigned char mac_bin[MAC_ADDR_LEN];
    unsigned char magic_packet[PACKET_LEN];

    if (!raw_mac || !broadcast_ip || !options || !had_error || !sent_count) {
        return 0;
    }

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

    if (options->prompt_enabled &&
        !confirm_send(normalized_mac, broadcast_ip, port)) {
        if (!options->quiet) {
            printf("Cancelled for %s.\n", normalized_mac);
        }
        return 0;
    }

    for (int attempt = 0; attempt < options->repeat_count; attempt++) {
        if (options->dry_run) {
            if (!options->quiet) {
                printf("DRY RUN: would send to %s via %s:%d (attempt %d/%d)\n",
                       normalized_mac, broadcast_ip, port, attempt + 1,
                       options->repeat_count);
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
                fprintf(stderr,
                        "Failed to send magic packet to %s (attempt %d/%d)\n",
                        normalized_mac, attempt + 1, options->repeat_count);
                *had_error = 1;
                return 0;
            }

            if (!options->quiet) {
                printf("Magic packet sent to %s (attempt %d/%d)\n",
                       normalized_mac, attempt + 1, options->repeat_count);
            }
            (*sent_count)++;
        }

        if (attempt + 1 < options->repeat_count && options->interval_ms > 0) {
            sleep_ms(options->interval_ms);
        }
    }

    return 1;
}
