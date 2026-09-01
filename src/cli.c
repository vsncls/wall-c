#include "wall.h"
#include <stdio.h>
#include <string.h>

/* Read exactly one line from stdin as a MAC address. */
int read_mac_from_stdin(char *mac_buf, size_t mac_buf_size) {
    size_t line_len;

    if (!mac_buf || mac_buf_size < 2) {
        return -1;
    }

    if (!fgets(mac_buf, mac_buf_size, stdin)) {
        return feof(stdin) ? 0 : -1;
    }

    /* A line that does not fit is rejected instead of being partially parsed. */
    if (!strchr(mac_buf, '\n') && !strchr(mac_buf, '\r') && !feof(stdin)) {
        return -1;
    }

    line_len = strcspn(mac_buf, "\r\n");
    mac_buf[line_len] = '\0';
    return line_len > 0 ? 1 : 0;
}

/* Print help text for CLI arguments. */
void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s [-m <mac_address>] [-b <broadcast_ip>] "
            "[--interface <name>] [-p <port>] [--count <n>] "
            "[--interval-ms <ms>] [-y] [-h] [--version] [--dry-run] "
            "[--quiet] [--smart]\n",
            prog_name);
    fprintf(stderr, "  -m <mac_address>    : MAC address "
                    "(XX:XX:XX:XX:XX:XX, XX-XX-XX-XX-XX-XX, or XXXXXXXXXXXX)\n");
    fprintf(stderr,
            "  -b <broadcast_ip>   : Broadcast IP address "
            "(default: 255.255.255.255)\n");
    fprintf(stderr, "  -p <port>           : Port number (default: 9)\n");
    fprintf(stderr, "  -y                  : Skip confirmation prompt\n");
    fprintf(stderr, "  -h                  : Display this help message\n");
    fprintf(stderr, "  --interface <name>  : Resolve broadcast from interface (e.g., en0)\n");
    fprintf(stderr, "  --version           : Display version\n");
    fprintf(stderr, "  --dry-run           : Validate and print actions without sending\n");
    fprintf(stderr, "  --quiet             : Reduce normal output\n");
    fprintf(stderr, "  --count <n>         : Send packet n times (default: 1)\n");
    fprintf(stderr, "  --interval-ms <ms>  : Delay between repeat sends (default: 0)\n");
    fprintf(stderr, "  --smart             : Skip send if host appears awake and verify wake after send\n");
    fprintf(stderr, "\nTarget source:\n");
    fprintf(stderr, "  1) -m/--mac <mac>\n");
    fprintf(stderr, "  2) one MAC from stdin\n");
}

void print_version(void) { printf("wall-c %s\n", WALL_C_VERSION); }

/* Interactive send confirmation. Returns 1 for yes, 0 otherwise. */
int confirm_send(const char *mac, const char *broadcast_ip, int port) {
    int c;

    printf("Send WoL packet?\n");
    printf("  MAC:       %s\n", mac);
    printf("  Broadcast: %s\n", broadcast_ip);
    printf("  Port:      %d\n", port);
    printf("Confirm [y/N]: ");
    fflush(stdout);

    c = getchar();
    while (getchar() != '\n' && !feof(stdin)) {
        ;
    }

    return (c == 'y' || c == 'Y');
}
