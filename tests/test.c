#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "wall.h"

static const char *C_RESET = "";
static const char *C_GREEN = "";
static const char *C_BLUE = "";

static void init_colors(void) {
    if (isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL) {
        C_RESET = "\x1b[0m";
        C_GREEN = "\x1b[32m";
        C_BLUE = "\x1b[34m";
    }
}

static void print_section(const char *msg) {
    printf("%s%s%s\n", C_BLUE, msg, C_RESET);
}

static void print_success(const char *msg) {
    printf("%s%s%s\n", C_GREEN, msg, C_RESET);
}

static void test_magic_packet_layout(void) {
    unsigned char mac[MAC_ADDR_LEN] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    unsigned char packet[PACKET_LEN];

    print_section("Testing magic packet byte layout...");
    build_magic_packet(mac, packet);

    for (int i = 0; i < 6; i++) {
        assert(packet[i] == 0xFF);
    }

    for (int repeat = 0; repeat < 16; repeat++) {
        for (int byte = 0; byte < MAC_ADDR_LEN; byte++) {
            int index = 6 + (repeat * MAC_ADDR_LEN) + byte;
            assert(packet[index] == mac[byte]);
        }
    }

    print_success("Magic packet layout test passed!");
}

static void test_validation(void) {
    char normalized[18];
    unsigned char mac_bin[MAC_ADDR_LEN];

    print_section("Testing validation functions...");

    assert(validate_mac("AA:BB:CC:DD:EE:FF") == 1);
    assert(validate_mac("00:11:22:33:44:55") == 1);
    assert(validate_mac("invalid_mac") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE:FG") == 0);

    assert(validate_ip("192.168.1.255") == 1);
    assert(validate_ip("127.0.0.1") == 1);
    assert(validate_ip("invalid_ip") == 0);
    assert(validate_ip("192.168.1.256") == 0);

    assert(validate_port(1) == 1);
    assert(validate_port(65535) == 1);
    assert(validate_port(0) == 0);
    assert(validate_port(65536) == 0);

    assert(normalize_mac("AA:BB:CC:DD:EE:FF", normalized,
                         sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);
    assert(normalize_mac("aa-bb-cc-dd-ee-ff", normalized,
                         sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);
    assert(normalize_mac("aabbccddeeff", normalized, sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);
    assert(normalize_mac("AA:BB-CC:DD-EE:FF", normalized,
                         sizeof(normalized)) == 0);

    assert(parse_mac("AA:BB:CC:DD:EE:FF", mac_bin) == 1);
    assert(mac_bin[0] == 0xAA && mac_bin[5] == 0xFF);
    assert(parse_mac("GG:BB:CC:DD:EE:FF", mac_bin) == 0);

    assert(should_prompt_for_confirmation(0, 1) == 1);
    assert(should_prompt_for_confirmation(1, 1) == 0);
    assert(should_prompt_for_confirmation(0, 0) == 0);

    print_success("All validation tests passed!");
}

static void test_stdin_read(void) {
    FILE *tmp;
    int saved_stdin;
    char mac[MAX_MAC_INPUT_LEN];

    print_section("Testing stdin MAC reading...");

    tmp = tmpfile();
    assert(tmp != NULL);
    assert(fputs("aa-bb-cc-dd-ee-ff\n", tmp) >= 0);
    rewind(tmp);

    saved_stdin = dup(STDIN_FILENO);
    assert(saved_stdin >= 0);
    assert(dup2(fileno(tmp), STDIN_FILENO) >= 0);

    assert(read_mac_from_stdin(mac, sizeof(mac)) == 1);
    assert(strcmp(mac, "aa-bb-cc-dd-ee-ff") == 0);

    assert(dup2(saved_stdin, STDIN_FILENO) >= 0);
    close(saved_stdin);
    fclose(tmp);

    print_success("Stdin MAC reading test passed!");
}

int main(void) {
    init_colors();
    test_validation();
    test_magic_packet_layout();
    test_stdin_read();
    return 0;
}
