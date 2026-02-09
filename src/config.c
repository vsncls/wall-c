#include "wall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE_NAME "wall-c/config"
#define MAX_MAC_LEN 128

/*
 * Read a MAC address from the XDG config location.
 *
 * Resolution order:
 * - $XDG_CONFIG_HOME/wall-c/config
 * - $HOME/.config/wall-c/config
 *
 * Returns a heap-allocated trimmed string or NULL when unavailable.
 */
char *read_mac_from_config(void) {
    char *config_path = NULL;
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    size_t path_size = 0;
    FILE *file = NULL;
    char *mac = NULL;

    if (xdg_config_home && xdg_config_home[0] != '\0') {
        path_size = strlen(xdg_config_home) + strlen(CONFIG_FILE_NAME) + 2;
        config_path = malloc(path_size);
        if (!config_path) {
            return NULL;
        }
        if (snprintf(config_path, path_size, "%s/%s", xdg_config_home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return NULL;
        }
    } else if (home && home[0] != '\0') {
        /* "/.config/" is 9 chars, plus null terminator. */
        path_size = strlen(home) + strlen(CONFIG_FILE_NAME) + 10;
        config_path = malloc(path_size);
        if (!config_path) {
            return NULL;
        }
        if (snprintf(config_path, path_size, "%s/.config/%s", home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return NULL;
        }
    } else {
        return NULL;
    }

    file = fopen(config_path, "r");
    free(config_path);
    if (!file) {
        return NULL;
    }

    mac = malloc(MAX_MAC_LEN);
    if (!mac) {
        fclose(file);
        return NULL;
    }

    if (fgets(mac, MAX_MAC_LEN, file)) {
        size_t len = strlen(mac);

        /* Trim trailing whitespace/newline characters. */
        while (len > 0 && (mac[len - 1] == '\n' || mac[len - 1] == '\r' ||
                           mac[len - 1] == ' ' || mac[len - 1] == '\t')) {
            mac[--len] = '\0';
        }

        /* Trim leading spaces/tabs by shifting the string in place. */
        char *start = mac;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (start != mac) {
            memmove(mac, start, strlen(start) + 1);
        }

        fclose(file);
        return mac;
    }

    free(mac);
    fclose(file);
    return NULL;
}
