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
int validate_ip(const char *ip);
int validate_port(int port);
void parse_mac(const char *mac_str, unsigned char *mac_bin);
void build_magic_packet(const unsigned char *mac, unsigned char *packet);
int send_wol_packet(const char *broadcast_ip, int port,
                    const unsigned char *packet, size_t packet_len);

void test_validation();
void test_config_read();

#endif
