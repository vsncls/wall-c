#include "wall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE_NAME "wall-c/config"

static int build_config_path(char **out_path) {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    size_t path_size = 0;
    char *config_path = NULL;

    if (!out_path) {
        return -1;
    }
    *out_path = NULL;

    if (xdg_config_home && xdg_config_home[0] != '\0') {
        path_size = strlen(xdg_config_home) + strlen(CONFIG_FILE_NAME) + 2;
        config_path = malloc(path_size);
        if (!config_path) {
            return -1;
        }
        if (snprintf(config_path, path_size, "%s/%s", xdg_config_home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return -1;
        }
    } else if (home && home[0] != '\0') {
        /* "/.config/" is 9 chars, plus null terminator. */
        path_size = strlen(home) + strlen(CONFIG_FILE_NAME) + 10;
        config_path = malloc(path_size);
        if (!config_path) {
            return -1;
        }
        if (snprintf(config_path, path_size, "%s/.config/%s", home,
                     CONFIG_FILE_NAME) < 0) {
            free(config_path);
            return -1;
        }
    } else {
        return 0;
    }

    *out_path = config_path;
    return 1;
}

static void trim_whitespace(char *s) {
    size_t len;
    char *start;

    if (!s) {
        return;
    }

    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }

    start = s;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

static int append_mac(mac_list_t *list, const char *line) {
    char **new_items;
    size_t line_len;
    char *copy;

    if (!list || !line) {
        return -1;
    }

    line_len = strlen(line);
    copy = malloc(line_len + 1);
    if (!copy) {
        return -1;
    }
    memcpy(copy, line, line_len + 1);

    new_items = realloc(list->items, sizeof(char *) * (list->count + 1));
    if (!new_items) {
        free(copy);
        return -1;
    }

    list->items = new_items;
    list->items[list->count] = copy;
    list->count++;
    return 0;
}

int read_macs_from_config(mac_list_t *list) {
    int path_status;
    char *config_path = NULL;
    FILE *file = NULL;
    char line[MAX_MAC_INPUT_LEN];

    if (!list) {
        return -1;
    }
    list->items = NULL;
    list->count = 0;

    path_status = build_config_path(&config_path);
    if (path_status <= 0) {
        return path_status;
    }

    file = fopen(config_path, "r");
    free(config_path);
    if (!file) {
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        trim_whitespace(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (append_mac(list, line) != 0) {
            fclose(file);
            free_mac_list(list);
            return -1;
        }
    }

    fclose(file);
    return (int)list->count;
}

void free_mac_list(mac_list_t *list) {
    if (!list) {
        return;
    }

    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

int read_mac_from_stdin(char *mac_buf, size_t mac_buf_size) {
    char line[MAX_MAC_INPUT_LEN];

    if (!mac_buf || mac_buf_size == 0) {
        return -1;
    }

    while (fgets(line, sizeof(line), stdin)) {
        trim_whitespace(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (strlen(line) + 1 > mac_buf_size) {
            return -1;
        }
        memcpy(mac_buf, line, strlen(line) + 1);
        return 1;
    }

    return 0;
}

/*
 * Backward-compatible helper retained for tests and old call sites.
 * Returns the first configured MAC or NULL.
 */
char *read_mac_from_config(void) {
    mac_list_t list;
    char *first = NULL;
    size_t len;
    int rc = read_macs_from_config(&list);

    if (rc <= 0) {
        return NULL;
    }

    len = strlen(list.items[0]);
    first = malloc(len + 1);
    if (first) {
        memcpy(first, list.items[0], len + 1);
    }
    free_mac_list(&list);
    return first;
}
