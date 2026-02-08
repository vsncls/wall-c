#ifndef WALL_H
#define WALL_H

/*
 * Shared headers for runtime and tests.
 * Centralizing includes keeps translation units consistent.
 */
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

/* Read a MAC address from $XDG_CONFIG_HOME or $HOME/.config fallback. */
char *read_mac_from_config(void);

/* Validate canonical MAC form: XX:XX:XX:XX:XX:XX. */
int validate_mac(const char *mac);

/*
 * Normalize supported MAC forms into canonical uppercase colon format.
 * Supported inputs:
 * - XX:XX:XX:XX:XX:XX
 * - XX-XX-XX-XX-XX-XX
 * - XXXXXXXXXXXX
 */
int normalize_mac(const char *input_mac, char *normalized_mac,
                  size_t normalized_size);

/* Validate dotted IPv4 address. */
int validate_ip(const char *ip);

/* Validate port range [1, 65535]. */
int validate_port(int port);

/* Prompt only when interactive and not explicitly skipped. */
int should_prompt_for_confirmation(int skip_confirm, int stdin_is_tty);

/* Print CLI usage text. */
void print_usage(const char *prog_name);

/* Interactive confirmation prompt. Returns 1 for yes, 0 otherwise. */
int confirm_send(const char *mac, const char *broadcast_ip, int port);

/* Parse canonical MAC string into 6 bytes. Returns 1 on success. */
int parse_mac(const char *mac_str, unsigned char *mac_bin);

/* Build WoL magic packet: 6 x 0xFF then 16 repetitions of MAC bytes. */
void build_magic_packet(const unsigned char *mac, unsigned char *packet);

/* Send packet over UDP broadcast. */
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len);

/* Unit test entry points. */
void test_validation(void);
void test_config_read(void);

#endif
