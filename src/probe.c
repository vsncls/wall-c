#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#include "wall.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#if defined(__APPLE__)
#include <sys/types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <sys/sysctl.h>
#endif

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

#if defined(__APPLE__)
static size_t sockaddr_advance(size_t len) {
    size_t align = sizeof(uintptr_t) - 1;
    size_t used = len == 0 ? sizeof(uintptr_t) : len;
    return (used + align) & ~align;
}
#endif

/*
 * Best-effort probe without spawning subprocesses.
 * Linux: parse /proc/net/arp and match MAC entries.
 * macOS: parse route/ARP entries from sysctl PF_ROUTE table.
 * Other platforms: unsupported and return -1.
 */
int probe_is_host_awake(const char *normalized_mac, const char *broadcast_ip,
                        int port, int timeout_ms) {
    (void)broadcast_ip;
    (void)port;
    (void)timeout_ms;

    if (!normalized_mac) {
        return -1;
    }

#if defined(__linux__)
    FILE *file = NULL;
    char line[512];

    file = fopen("/proc/net/arp", "r");
    if (!file) {
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char ip[64] = {0};
        char hw_type[16] = {0};
        char flags[16] = {0};
        char hw_addr[64] = {0};
        char mask[64] = {0};
        char device[64] = {0};
        int fields = sscanf(line, "%63s %15s %15s %63s %63s %63s", ip, hw_type,
                            flags, hw_addr, mask, device);
        if (fields != 6) {
            continue;
        }
        if (strcasecmp(hw_addr, normalized_mac) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
#elif defined(__APPLE__)
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
    size_t needed = 0;
    char *buffer = NULL;
    char *next = NULL;
    char *limit = NULL;
    unsigned char target_mac[MAC_ADDR_LEN];

    if (!parse_mac(normalized_mac, target_mac)) {
        return -1;
    }

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
        return -1;
    }
    if (needed == 0) {
        return 0;
    }

    buffer = malloc(needed);
    if (!buffer) {
        return -1;
    }

    if (sysctl(mib, 6, buffer, &needed, NULL, 0) < 0) {
        free(buffer);
        return -1;
    }

    limit = buffer + needed;
    for (next = buffer; next < limit;) {
        struct rt_msghdr *rtm = (struct rt_msghdr *)next;
        struct sockaddr *sa = NULL;
        struct sockaddr_dl *sdl = NULL;
        int addrs = 0;

        if (rtm->rtm_msglen == 0) {
            break;
        }

        sa = (struct sockaddr *)(rtm + 1);
        addrs = rtm->rtm_addrs;
        for (int i = 0; i < RTAX_MAX; i++) {
            if ((addrs & (1 << i)) != 0) {
                if (i == RTAX_GATEWAY && sa->sa_family == AF_LINK) {
                    sdl = (struct sockaddr_dl *)sa;
                }
                sa = (struct sockaddr *)((char *)sa +
                                         sockaddr_advance(sa->sa_len));
            }
        }

        if (sdl && sdl->sdl_alen == MAC_ADDR_LEN) {
            unsigned char *entry_mac = (unsigned char *)LLADDR(sdl);
            if (memcmp(entry_mac, target_mac, MAC_ADDR_LEN) == 0) {
                free(buffer);
                return 1;
            }
        }

        next += rtm->rtm_msglen;
    }

    free(buffer);
    return 0;
#else
    return -1;
#endif
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
