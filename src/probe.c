#include "wall.h"

/*
 * Probe stage placeholder.
 * Smart wake policy is not enabled yet, so these intentionally behave as
 * "unknown/offline" and do not affect send behavior.
 */
int probe_is_host_awake(const char *normalized_mac, const char *broadcast_ip,
                        int port, int timeout_ms) {
    (void)normalized_mac;
    (void)broadcast_ip;
    (void)port;
    (void)timeout_ms;
    return 0;
}

int probe_wait_for_host_awake(const char *normalized_mac,
                              const char *broadcast_ip, int port,
                              int timeout_ms, int interval_ms) {
    (void)normalized_mac;
    (void)broadcast_ip;
    (void)port;
    (void)timeout_ms;
    (void)interval_ms;
    return 0;
}
