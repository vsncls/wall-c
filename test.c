#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wall.h"

void test_validation() {
    printf("Testing validation functions...\n");
    assert(validate_mac("AA:BB:CC:DD:EE:FF") == 1);
    assert(validate_mac("00:11:22:33:44:55") == 1);
    assert(validate_mac("invalid_mac") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE:FF:") == 0);
    assert(validate_mac("AA:BB:CC:DD:EE:FG") == 0);

    assert(validate_ip("192.168.1.255") == 1);
    assert(validate_ip("127.0.0.1") == 1);
    assert(validate_ip("invalid_ip") == 0);
    assert(validate_ip("192.168.1.256") == 0);
    assert(validate_ip("192.168.1") == 0);

    assert(validate_port(1) == 1);
    assert(validate_port(65535) == 1);
    assert(validate_port(0) == 0);
    assert(validate_port(65536) == 0);
    printf("All validation tests passed!\n");
}

void test_config_read() {
    printf("Testing config file reading...\n");
    
    // Create a temporary test config directory
    char test_config_dir[] = "/tmp/wall-c-test-XXXXXX";
    if (!mkdtemp(test_config_dir)) {
        perror("Failed to create temp directory");
        return;
    }
    
    char test_config_file[256];
    snprintf(test_config_file, sizeof(test_config_file), "%s/wall-c", test_config_dir);
    mkdir(test_config_file, 0755);
    
    char test_config_path[256];
    snprintf(test_config_path, sizeof(test_config_path), "%s/wall-c/config", test_config_dir);
    
    // Test 1: Valid MAC in config
    FILE *f = fopen(test_config_path, "w");
    fprintf(f, "AA:BB:CC:DD:EE:FF\n");
    fclose(f);
    
    setenv("XDG_CONFIG_HOME", test_config_dir, 1);
    char *mac = read_mac_from_config();
    assert(mac != NULL);
    assert(strcmp(mac, "AA:BB:CC:DD:EE:FF") == 0);
    free(mac);
    printf("  ✓ Valid MAC from config file\n");
    
    // Test 2: MAC with whitespace
    f = fopen(test_config_path, "w");
    fprintf(f, "  11:22:33:44:55:66  \n");
    fclose(f);
    
    mac = read_mac_from_config();
    assert(mac != NULL);
    assert(strcmp(mac, "11:22:33:44:55:66") == 0);
    free(mac);
    printf("  ✓ MAC with whitespace trimmed\n");
    
    // Test 3: Malicious content in config file (shell injection attempts)
    const char *malicious_inputs[] = {
        "AA:BB:CC:DD:EE:FF; echo pwned",
        "$(whoami)",
        "`id`",
        "AA:BB:CC:DD:EE:FF && ls",
        "| cat /etc/hosts",
        NULL
    };
    for (int i = 0; malicious_inputs[i] != NULL; i++) {
        f = fopen(test_config_path, "w");
        fprintf(f, "%s\n", malicious_inputs[i]);
        fclose(f);
        
        mac = read_mac_from_config();
        assert(mac != NULL); // File is read
        assert(validate_mac(mac) == 0); // But validation rejects it
        free(mac);
    }
    printf("  ✓ Malicious config content rejected by validation\n");
    
    // Test 4: Empty config file
    f = fopen(test_config_path, "w");
    fclose(f);
    
    mac = read_mac_from_config();
    assert(mac == NULL);
    printf("  ✓ Empty config file returns NULL\n");
    
    // Test 5: Non-existent config file
    unlink(test_config_path);
    mac = read_mac_from_config();
    assert(mac == NULL);
    printf("  ✓ Missing config file returns NULL\n");
    
    // Cleanup
    rmdir(test_config_file);
    rmdir(test_config_dir);
    unsetenv("XDG_CONFIG_HOME");
    
    printf("All config reading tests passed!\n");
}
