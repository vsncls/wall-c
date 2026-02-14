#include "wall.h"

void build_magic_packet(const unsigned char *mac, unsigned char *packet) {
    /* WoL magic packet starts with 6 bytes of 0xFF. */
    memset(packet, 0xFF, 6);
    /*
     * Then append the target MAC address 16 times.
     * Total packet layout:
     * - 6 bytes prefix
     * - 16 repetitions * 6-byte MAC = 96 bytes
     * - 102 bytes total (PACKET_LEN)
     */
    for (int i = 0; i < 16; i++) {
        memcpy(packet + 6 + (i * MAC_ADDR_LEN), mac, MAC_ADDR_LEN);
    }
}
