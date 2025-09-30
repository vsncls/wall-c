#include <assert.h>
#include <stdio.h>

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
