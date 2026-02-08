#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wall.h"

/*
 * This test binary focuses on pure helpers and config path behavior.
 * Networking is intentionally not exercised here to keep tests deterministic.
 */

static void fail_with_errno(const char *context) {
    perror(context);
    exit(EXIT_FAILURE);
}

static void *require_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fail_with_errno("malloc");
    }
    return ptr;
}

/*
 * Build an exact-size "a + b" path/string allocation.
 * This avoids truncation-prone fixed buffers in tests.
 */
static char *concat2(const char *a, const char *b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char *out = require_malloc(len_a + len_b + 1);

    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    out[len_a + len_b] = '\0';
    return out;
}

/* Build an exact-size "a + b + c" allocation. */
static char *concat3(const char *a, const char *b, const char *c) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t len_c = strlen(c);
    char *out = require_malloc(len_a + len_b + len_c + 1);

    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    memcpy(out + len_a + len_b, c, len_c);
    out[len_a + len_b + len_c] = '\0';
    return out;
}

/*
 * Portable temp directory creator that does not depend on mkdtemp visibility.
 * Creates a unique path by using mkstemp, then replaces the file with a dir.
 */
static void create_temp_dir(char *path_buf, size_t path_buf_size,
                            const char *prefix) {
    int fd;

    if (snprintf(path_buf, path_buf_size, "%sXXXXXX", prefix) >=
        (int)path_buf_size) {
        fprintf(stderr, "Temp path buffer too small\n");
        exit(EXIT_FAILURE);
    }

    fd = mkstemp(path_buf);
    if (fd < 0) {
        fail_with_errno("mkstemp");
    }
    if (close(fd) != 0) {
        fail_with_errno("close");
    }
    if (unlink(path_buf) != 0) {
        fail_with_errno("unlink");
    }
    if (mkdir(path_buf, 0755) != 0) {
        fail_with_errno("mkdir");
    }
}

static void require_mkdir(const char *path) {
    if (mkdir(path, 0755) != 0) {
        fail_with_errno("mkdir");
    }
}

static FILE *require_fopen(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    if (!f) {
        fail_with_errno("fopen");
    }
    return f;
}

static void require_fclose(FILE *f) {
    if (fclose(f) != 0) {
        fail_with_errno("fclose");
    }
}

static void write_config_line(const char *path, const char *line) {
    FILE *f = require_fopen(path, "w");
    if (fprintf(f, "%s\n", line) < 0) {
        fail_with_errno("fprintf");
    }
    require_fclose(f);
}

static void write_empty_file(const char *path) {
    FILE *f = require_fopen(path, "w");
    require_fclose(f);
}

static void require_setenv(const char *key, const char *value, int overwrite) {
    if (setenv(key, value, overwrite) != 0) {
        fail_with_errno("setenv");
    }
}

static void require_unsetenv(const char *key) {
    if (unsetenv(key) != 0) {
        fail_with_errno("unsetenv");
    }
}

static void require_unlink(const char *path) {
    if (unlink(path) != 0) {
        fail_with_errno("unlink");
    }
}

static void require_rmdir(const char *path) {
    if (rmdir(path) != 0) {
        fail_with_errno("rmdir");
    }
}

/*
 * Verify packet structure exactly matches WoL spec:
 * - bytes [0..5] are 0xFF
 * - bytes [6..101] are 16 repeats of the target MAC
 */
static void test_magic_packet_layout(void) {
    unsigned char mac[MAC_ADDR_LEN] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    unsigned char packet[PACKET_LEN];

    printf("Testing magic packet byte layout...\n");
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

    printf("Magic packet layout test passed!\n");
}

/* Validate format-checking helpers with representative valid/invalid inputs. */
void test_validation() {
    printf("Testing validation functions...\n");

    /* Canonical MAC format acceptance and rejection cases. */
    assert(validate_mac("AA:BB:CC:DD:EE:FF") == 1);
    assert(validate_mac("00:11:22:33:44:55") == 1);
    assert(validate_mac("invalid_mac") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE:FF:") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE:FG") == 0);

    /* IPv4 validation accepts real addresses and rejects malformed strings. */
    assert(validate_ip("192.168.1.255") == 1);
    assert(validate_ip("127.0.0.1") == 1);
    assert(validate_ip("invalid_ip") == 0);
    assert(validate_ip("192.168.1.256") == 0);
    assert(validate_ip("192.168.1") == 0);

    /* Port range boundaries for valid UDP destination values. */
    assert(validate_port(1) == 1);
    assert(validate_port(65535) == 1);
    assert(validate_port(0) == 0);
    assert(validate_port(65536) == 0);

    /* Normalization accepts supported forms and returns canonical output. */
    char normalized[18];
    assert(normalize_mac("AA:BB:CC:DD:EE:FF", normalized,
                         sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);
    assert(normalize_mac("aa-bb-cc-dd-ee-ff", normalized,
                         sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);
    assert(normalize_mac("aabbccddeeff", normalized, sizeof(normalized)) == 1);
    assert(strcmp(normalized, "AA:BB:CC:DD:EE:FF") == 0);

    /* Normalization rejects malformed forms. */
    assert(normalize_mac("AA:BB:CC:DD:EE", normalized, sizeof(normalized)) == 0);
    assert(normalize_mac("AA/BB/CC/DD/EE/FF", normalized, sizeof(normalized)) == 0);
    assert(normalize_mac("AA:BB-CC:DD-EE:FF", normalized, sizeof(normalized)) == 0);
    assert(normalize_mac("GG:BB:CC:DD:EE:FF", normalized, sizeof(normalized)) == 0);

    /* Parser succeeds for normalized input and fails for non-hex input. */
    unsigned char mac_bin[MAC_ADDR_LEN];
    assert(parse_mac("AA:BB:CC:DD:EE:FF", mac_bin) == 1);
    assert(mac_bin[0] == 0xAA && mac_bin[5] == 0xFF);
    assert(parse_mac("GG:BB:CC:DD:EE:FF", mac_bin) == 0);

    /* Non-interactive confirmation policy is deterministic. */
    assert(should_prompt_for_confirmation(0, 1) == 1);
    assert(should_prompt_for_confirmation(1, 1) == 0);
    assert(should_prompt_for_confirmation(0, 0) == 0);
    assert(should_prompt_for_confirmation(1, 0) == 0);
    printf("All validation tests passed!\n");
}

/*
 * Validate XDG config file parsing behavior.
 * The test creates isolated temporary files to avoid mutating user config.
 */
void test_config_read() {
    printf("Testing config file reading...\n");
    
    /* Create isolated temp directory root, then wall-c/config beneath it. */
    char test_config_dir[256];
    create_temp_dir(test_config_dir, sizeof(test_config_dir), "/tmp/wall-c-test-");
    
    char *test_config_file = concat2(test_config_dir, "/wall-c");
    require_mkdir(test_config_file);
    
    char *test_config_path = concat2(test_config_file, "/config");
    
    /* Test 1: canonical MAC should be read exactly as stored. */
    write_config_line(test_config_path, "AA:BB:CC:DD:EE:FF");
    
    /* Point config lookup at the temp tree via XDG_CONFIG_HOME override. */
    require_setenv("XDG_CONFIG_HOME", test_config_dir, 1);
    char *mac = read_mac_from_config();
    assert(mac != NULL);
    assert(strcmp(mac, "AA:BB:CC:DD:EE:FF") == 0);
    free(mac);
    printf("  ✓ Valid MAC from config file\n");
    
    /* Test 2: leading/trailing whitespace should be trimmed in-place. */
    write_config_line(test_config_path, "  11:22:33:44:55:66  ");
    
    mac = read_mac_from_config();
    assert(mac != NULL);
    assert(strcmp(mac, "11:22:33:44:55:66") == 0);
    free(mac);
    printf("  ✓ MAC with whitespace trimmed\n");
    
    /*
     * Test 3: suspicious input should be read as plain text and then rejected
     * by format validation. This verifies there is no shell evaluation path.
     */
    const char *malicious_inputs[] = {
        "AA:BB:CC:DD:EE:FF; echo pwned",
        "$(whoami)",
        "`id`",
        "AA:BB:CC:DD:EE:FF && ls",
        "| cat /etc/hosts",
        NULL
    };
    for (int i = 0; malicious_inputs[i] != NULL; i++) {
        write_config_line(test_config_path, malicious_inputs[i]);
        
        mac = read_mac_from_config();
        assert(mac != NULL); // File is read
        assert(validate_mac(mac) == 0); // But validation rejects it
        free(mac);
    }
    printf("  ✓ Malicious config content rejected by validation\n");
    
    /* Test 4: empty file has no first line and should return NULL. */
    write_empty_file(test_config_path);
    
    mac = read_mac_from_config();
    assert(mac == NULL);
    printf("  ✓ Empty config file returns NULL\n");
    
    /* Test 5: missing file should also return NULL, not hard-fail. */
    require_unlink(test_config_path);
    mac = read_mac_from_config();
    assert(mac == NULL);
    printf("  ✓ Missing config file returns NULL\n");
    
    /* Cleanup temp directories and restore process environment. */
    require_rmdir(test_config_file);
    require_rmdir(test_config_dir);
    require_unsetenv("XDG_CONFIG_HOME");
    free(test_config_path);
    free(test_config_file);
    
    printf("All config reading tests passed!\n");
}

/*
 * Validate HOME-based fallback path with a deliberately long HOME value.
 * This protects against path buffer math regressions.
 */
static void test_long_home_path() {
    printf("Testing long HOME path config resolution...\n");

    /* Build temporary directory root used as parent for long HOME. */
    char base_dir[256];
    create_temp_dir(base_dir, sizeof(base_dir), "/tmp/wall-c-home-test-");

    /* Generate a long directory segment to stress path construction. */
    char long_segment[128];
    memset(long_segment, 'a', sizeof(long_segment) - 1);
    long_segment[sizeof(long_segment) - 1] = '\0';

    char *long_home = concat3(base_dir, "/", long_segment);
    require_mkdir(long_home);

    /* Create ~/.config/wall-c/config structure beneath synthetic HOME. */
    char *xdg_dir = concat2(long_home, "/.config");
    require_mkdir(xdg_dir);

    char *config_dir = concat2(xdg_dir, "/wall-c");
    require_mkdir(config_dir);

    char *config_path = concat2(config_dir, "/config");

    write_config_line(config_path, "AA:BB:CC:DD:EE:FF");

    /* Save current HOME so test does not leak environment mutations. */
    const char *old_home = getenv("HOME");
    char old_home_buf[512];
    if (old_home) {
        strncpy(old_home_buf, old_home, sizeof(old_home_buf) - 1);
        old_home_buf[sizeof(old_home_buf) - 1] = '\0';
    }

    /* Force fallback branch by unsetting XDG_CONFIG_HOME. */
    require_unsetenv("XDG_CONFIG_HOME");
    require_setenv("HOME", long_home, 1);

    /* Verify that HOME fallback resolves and parses config correctly. */
    char *mac = read_mac_from_config();
    assert(mac != NULL);
    assert(strcmp(mac, "AA:BB:CC:DD:EE:FF") == 0);
    free(mac);
    printf("  ✓ Long HOME path config read\n");

    /* Restore original HOME for subsequent tests or shell operations. */
    if (old_home) {
        require_setenv("HOME", old_home_buf, 1);
    } else {
        require_unsetenv("HOME");
    }

    /* Remove all files/directories created by this test. */
    require_unlink(config_path);
    require_rmdir(config_dir);
    require_rmdir(xdg_dir);
    require_rmdir(long_home);
    require_rmdir(base_dir);
    free(config_path);
    free(config_dir);
    free(xdg_dir);
    free(long_home);
}

/* Test runner entry point. */
int main(void) {
    test_validation();
    test_magic_packet_layout();
    test_config_read();
    test_long_home_path();
    return 0;
}
