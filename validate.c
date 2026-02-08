#include "wall.h"

/* Canonical validator used after normalization. */
int validate_mac(const char *mac) {
    if (strlen(mac) != 17) {
        return 0;
    }

    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':') {
                return 0;
            }
        } else if (!isxdigit((unsigned char)mac[i])) {
            return 0;
        }
    }

    return 1;
}

/*
 * Normalize supported MAC formats into canonical uppercase form.
 * Result: "AA:BB:CC:DD:EE:FF"
 */
int normalize_mac(const char *input_mac, char *normalized_mac,
                  size_t normalized_size) {
    char hex[13];
    size_t hex_index = 0;
    size_t len;

    if (!input_mac || !normalized_mac || normalized_size < 18) {
        return 0;
    }

    len = strlen(input_mac);
    if (len == 17) {
        char sep = input_mac[2];

        if (sep != ':' && sep != '-') {
            return 0;
        }

        for (size_t i = 0; i < len; i++) {
            if (i % 3 == 2) {
                if (input_mac[i] != sep) {
                    return 0;
                }
            } else {
                if (!isxdigit((unsigned char)input_mac[i])) {
                    return 0;
                }
                hex[hex_index++] =
                    (char)toupper((unsigned char)input_mac[i]);
            }
        }
    } else if (len == 12) {
        for (size_t i = 0; i < len; i++) {
            if (!isxdigit((unsigned char)input_mac[i])) {
                return 0;
            }
            hex[hex_index++] = (char)toupper((unsigned char)input_mac[i]);
        }
    } else {
        return 0;
    }

    if (hex_index != 12) {
        return 0;
    }
    hex[hex_index] = '\0';

    if (snprintf(normalized_mac, normalized_size,
                 "%.2s:%.2s:%.2s:%.2s:%.2s:%.2s", hex, hex + 2, hex + 4,
                 hex + 6, hex + 8, hex + 10) < 0) {
        return 0;
    }

    return 1;
}

int validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

int validate_port(int port) { return port > 0 && port <= 65535; }

int should_prompt_for_confirmation(int skip_confirm, int stdin_is_tty) {
    return !skip_confirm && stdin_is_tty;
}

/* Parse normalized MAC text into 6 bytes. */
int parse_mac(const char *mac_str, unsigned char *mac_bin) {
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        unsigned int byte = 0;
        if (sscanf(mac_str + (i * 3), "%2x", &byte) != 1) {
            return 0;
        }
        mac_bin[i] = (unsigned char)byte;
    }
    return 1;
}
