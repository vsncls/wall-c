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
#define WALL_C_VERSION "0.3.0"
#define MAC_ADDR_LEN 6
#define PACKET_LEN 102
#define MAX_MAC_INPUT_LEN 128
#define MAX_CONFIG_LINE_LEN 512

typedef struct {
    char **items;
    size_t count;
} mac_list_t;

typedef struct {
    char *name;
    char *mac;
    char *broadcast_ip;
    int port;
} wake_target_t;

typedef struct {
    wake_target_t *items;
    size_t count;
} target_list_t;

typedef struct {
    int prompt_enabled;
    int repeat_count;
    int interval_ms;
    int dry_run;
    int quiet;
} wake_send_options_t;

/* Read a MAC address from $XDG_CONFIG_HOME or $HOME/.config fallback. */
char *read_mac_from_config(void);

/* Read all MAC addresses from config file (one per non-empty non-comment line). */
int read_macs_from_config(mac_list_t *list);

/* Read all wake targets from config file. */
int read_targets_from_config(target_list_t *list);

/* Release heap allocations created by read_targets_from_config. */
void free_target_list(target_list_t *list);

/* Release heap allocations created by read_macs_from_config. */
void free_mac_list(mac_list_t *list);

/* Read one MAC address from stdin (first non-empty non-comment line). */
int read_mac_from_stdin(char *mac_buf, size_t mac_buf_size);

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

/* Print short version text. */
void print_version(void);

/* Interactive confirmation prompt. Returns 1 for yes, 0 otherwise. */
int confirm_send(const char *mac, const char *broadcast_ip, int port);

/* Parse canonical MAC string into 6 bytes. Returns 1 on success. */
int parse_mac(const char *mac_str, unsigned char *mac_bin);

/* Build WoL magic packet: 6 x 0xFF then 16 repetitions of MAC bytes. */
void build_magic_packet(const unsigned char *mac, unsigned char *packet);

/* Send packet over UDP broadcast. */
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len);

/* Resolve interface broadcast IPv4 address into out_broadcast. */
int resolve_interface_broadcast(const char *ifname, char *out_broadcast,
                                size_t out_size);

/* Probe helpers for future smart-wake policy. */
int probe_is_host_awake(const char *normalized_mac, const char *broadcast_ip,
                        int port, int timeout_ms);
int probe_wait_for_host_awake(const char *normalized_mac,
                              const char *broadcast_ip, int port,
                              int timeout_ms, int interval_ms);

/* Engine entry for the send stage of the wake pipeline. */
int engine_process_target(const char *raw_mac, const char *broadcast_ip, int port,
                          const wake_send_options_t *options, int *had_error,
                          int *sent_count);

/* Unit test entry points. */
void test_validation(void);
void test_config_read(void);

#endif
