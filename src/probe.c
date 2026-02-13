#include "wall.h"
#include <ctype.h>
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

static void to_lower_copy(const char *src, char *dst, size_t dst_size) {
    size_t i = 0;

    if (!src || !dst || dst_size == 0) {
        return;
    }

    while (src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

/*
 * Best-effort probe: search the local ARP cache for the target MAC.
 * This is intentionally lightweight and cross-platform for macOS/Linux.
 */
int probe_is_host_awake(const char *normalized_mac, const char *broadcast_ip,
                        int port, int timeout_ms) {
    FILE *pipe = NULL;
    char command[] = "arp -an";
    char line[1024];
    char lowered_line[1024];
    char lowered_mac[18];
    int found = 0;

    (void)broadcast_ip;
    (void)port;
    (void)timeout_ms;

    if (!normalized_mac) {
        return -1;
    }

    to_lower_copy(normalized_mac, lowered_mac, sizeof(lowered_mac));

    pipe = popen(command, "r");
    if (!pipe) {
        return -1;
    }

    while (fgets(line, sizeof(line), pipe)) {
        to_lower_copy(line, lowered_line, sizeof(lowered_line));
        if (strstr(lowered_line, lowered_mac) != NULL) {
            found = 1;
            break;
        }
    }

    if (pclose(pipe) == -1) {
        return -1;
    }

    return found;
}

int probe_wait_for_host_awake(const char *normalized_mac,
                              const char *broadcast_ip, int port,
                              int timeout_ms, int interval_ms) {
    int elapsed_ms = 0;
    int effective_interval = interval_ms > 0 ? interval_ms : 250;

    if (!normalized_mac) {
        return -1;
    }

    if (timeout_ms <= 0) {
        return probe_is_host_awake(normalized_mac, broadcast_ip, port, 0);
    }

    while (elapsed_ms <= timeout_ms) {
        int state =
            probe_is_host_awake(normalized_mac, broadcast_ip, port, 0);
        if (state != 0) {
            return state;
        }
        sleep_ms(effective_interval);
        elapsed_ms += effective_interval;
    }

    return 0;
}
