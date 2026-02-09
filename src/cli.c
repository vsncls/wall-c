#include "wall.h"
#include <stdio.h>

/* Print help text for CLI arguments. */
void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s [-m <mac_address>] [--target <name>] [--list-targets] "
            "[-b <broadcast_ip>] [--interface <name>] [-p <port>] "
            "[--count <n>] [--interval-ms <ms>] [--continue-on-error] "
            "[-y] [-h] [--version] [--dry-run] [--quiet]\n",
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
    fprintf(stderr, "  --target <name>     : Wake one named target from config\n");
    fprintf(stderr,
            "  --list-targets      : Print parsed targets from config and exit\n");
    fprintf(stderr, "  --dry-run           : Validate and print actions without sending\n");
    fprintf(stderr, "  --quiet             : Reduce normal output\n");
    fprintf(stderr, "  --count <n>         : Send packet n times per target (default: 1)\n");
    fprintf(stderr, "  --interval-ms <ms>  : Delay between repeat sends (default: 0)\n");
    fprintf(stderr,
            "  --continue-on-error : Keep processing later targets after failures\n");
    fprintf(stderr, "\nTarget precedence:\n");
    fprintf(stderr, "  1) -m <mac>\n");
    fprintf(stderr, "  2) --target <name>\n");
    fprintf(stderr, "  3) first MAC from stdin\n");
    fprintf(stderr, "  4) all targets in config file\n");
    fprintf(stderr, "\nConfig file location: $XDG_CONFIG_HOME/wall-c/config "
                    "or ~/.config/wall-c/config\n");
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
