#include "wall.h"

void build_magic_packet(const unsigned char *mac, unsigned char *packet) {
    /* WoL magic packet starts with 6 bytes of 0xFF. */
    memset(packet, 0xFF, 6);
    /* Then append the target MAC address 16 times (16 * 6 bytes). */
    for (int i = 0; i < 16; i++) {
        memcpy(packet + 6 + (i * MAC_ADDR_LEN), mac, MAC_ADDR_LEN);
    }
}
