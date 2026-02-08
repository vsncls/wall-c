#ifndef WALL_H
#define WALL_H

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 9
#define MAC_ADDR_LEN 6
#define PACKET_LEN 102

char *read_mac_from_config(void);
int validate_mac(const char *mac);
<<<<<<< Updated upstream
=======

/*
 * Normalize supported MAC forms into canonical uppercase colon format.
 * Supported input forms:
 * - "AA:BB:CC:DD:EE:FF"
 * - "AA-BB-CC-DD-EE-FF"
 * - "AABBCCDDEEFF"
 */
int normalize_mac(const char *input_mac, char *normalized_mac,
                  size_t normalized_size);

/* Validate a dotted IPv4 address using inet_pton. */
>>>>>>> Stashed changes
int validate_ip(const char *ip);
int validate_port(int port);
<<<<<<< Updated upstream
void parse_mac(const char *mac_str, unsigned char *mac_bin);
=======

/*
 * Confirmation policy helper:
 * - prompt only when `-y` was not passed and stdin is interactive.
 */
int should_prompt_for_confirmation(int skip_confirm, int stdin_is_tty);

/* Convert text MAC into 6 raw bytes for packet construction. */
int parse_mac(const char *mac_str, unsigned char *mac_bin);

/* Build the 102-byte WoL magic packet (FF sync + 16x MAC). */
>>>>>>> Stashed changes
void build_magic_packet(const unsigned char *mac, unsigned char *packet);
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len);

void test_validation();
void test_config_read();

#endif
