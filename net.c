#include "wall.h"

int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len) {
    int sock;
    int broadcast_enable = 1;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                   sizeof(broadcast_enable)) < 0) {
        perror("Failed to enable broadcast");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, broadcast_ip, &addr.sin_addr) <= 0) {
        perror("Invalid broadcast IP");
        close(sock);
        return -1;
    }

    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *)&addr,
               sizeof(addr)) < 0) {
        perror("Failed to send packet");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}
