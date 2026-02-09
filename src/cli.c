#include "wall.h"
#include <stdio.h>

/* Print help text for CLI arguments. */
void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s [-m <mac_address>] [-b <broadcast_ip>] [-p <port>] "
            "[-y] [-h]\n",
            prog_name);
    fprintf(stderr, "  -m <mac_address>    : MAC address "
                    "(XX:XX:XX:XX:XX:XX, XX-XX-XX-XX-XX-XX, or XXXXXXXXXXXX)\n");
    fprintf(stderr,
            "  -b <broadcast_ip>   : Broadcast IP address "
            "(default: 255.255.255.255)\n");
    fprintf(stderr, "  -p <port>           : Port number (default: 9)\n");
    fprintf(stderr, "  -y                  : Skip confirmation prompt\n");
    fprintf(stderr, "  -h                  : Display this help message\n");
    fprintf(stderr, "\nTarget precedence:\n");
    fprintf(stderr, "  1) -m <mac>\n");
    fprintf(stderr, "  2) first MAC from stdin\n");
    fprintf(stderr, "  3) all MACs in config file (one per line)\n");
    fprintf(stderr, "\nConfig file location: $XDG_CONFIG_HOME/wall-c/config "
                    "or ~/.config/wall-c/config\n");
}

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
