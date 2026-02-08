#include "wall.h"

void build_magic_packet(const unsigned char *mac, unsigned char *packet) {
    memset(packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(packet + 6 + (i * MAC_ADDR_LEN), mac, MAC_ADDR_LEN);
    }
}
