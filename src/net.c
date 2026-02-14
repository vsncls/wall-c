#include "wall.h"
#include <ifaddrs.h>
#include <net/if.h>

int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len) {
    int sock;
    int broadcast_enable = 1;
    struct sockaddr_in addr;

    /* Wake-on-LAN is sent as a UDP datagram over IPv4 broadcast. */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    /* UDP broadcast is disabled by default, so enable it explicitly. */
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                   sizeof(broadcast_enable)) < 0) {
        perror("Failed to enable broadcast");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    /* Convert dotted text IP (e.g., "192.168.1.255") into binary form. */
    if (inet_pton(AF_INET, broadcast_ip, &addr.sin_addr) <= 0) {
        perror("Invalid broadcast IP");
        close(sock);
        return -1;
    }

    /*
     * sendto transmits one datagram to the broadcast endpoint.
     * packet_len is expected to be PACKET_LEN for WoL.
     */
    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&addr,
               sizeof(addr)) < 0) {
        perror("Failed to send packet");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

int resolve_interface_broadcast(const char *ifname, char *out_broadcast,
                                size_t out_size) {
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    int found = 0;

    if (!ifname || !out_broadcast || out_size == 0) {
        return -1;
    }

    if (getifaddrs(&ifaddr) != 0) {
        perror("getifaddrs failed");
        return -1;
    }

    /* Scan OS-reported interfaces until we find the requested IPv4 broadcast. */
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        struct sockaddr_in *bcast_addr = NULL;

        /* Keep only records that belong to the requested interface name. */
        if (!ifa->ifa_name || strcmp(ifa->ifa_name, ifname) != 0) {
            continue;
        }
        /* Some interface records do not carry broadcast information. */
        if (!ifa->ifa_broadaddr) {
            continue;
        }
        /* This resolver intentionally handles IPv4 only. */
        if (ifa->ifa_broadaddr->sa_family != AF_INET) {
            continue;
        }

        bcast_addr = (struct sockaddr_in *)ifa->ifa_broadaddr;
        /* Convert binary broadcast address back to readable dotted form. */
        if (!inet_ntop(AF_INET, &bcast_addr->sin_addr, out_broadcast,
                       out_size)) {
            continue;
        }

        found = 1;
        break;
    }

    freeifaddrs(ifaddr);
    return found;
}
