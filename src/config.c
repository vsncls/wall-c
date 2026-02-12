#include "wall.h"
#include <errno.h>
#include <limits.h>
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

    /* Prefer XDG config location when available. */
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
        /* Fallback to ~/.config if XDG_CONFIG_HOME is not set. */
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

    /* Trim trailing whitespace/newlines first. */
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }

    /* Then trim leading spaces/tabs by shifting the string left. */
    start = s;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

static char *dup_string(const char *s) {
    size_t len;
    char *out;

    if (!s) {
        return NULL;
    }

    len = strlen(s);
    out = malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static void free_single_target(wake_target_t *target) {
    if (!target) {
        return;
    }
    free(target->name);
    free(target->mac);
    free(target->broadcast_ip);
    target->name = NULL;
    target->mac = NULL;
    target->broadcast_ip = NULL;
    target->port = 0;
}

static int append_target(target_list_t *list, wake_target_t *target) {
    wake_target_t *new_items;

    if (!list || !target) {
        return -1;
    }

    new_items =
        realloc(list->items, sizeof(wake_target_t) * (list->count + 1));
    if (!new_items) {
        return -1;
    }

    list->items = new_items;
    list->items[list->count] = *target;
    list->count++;
    target->name = NULL;
    target->mac = NULL;
    target->broadcast_ip = NULL;
    target->port = 0;
    return 0;
}

static int append_mac(mac_list_t *list, const char *line) {
    char **new_items;
    char *copy = NULL;

    if (!list || !line) {
        return -1;
    }

    copy = dup_string(line);
    if (!copy) {
        return -1;
    }

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

static int parse_port_string(const char *token, int *out_port) {
    char *endptr = NULL;
    long parsed = 0;

    if (!token || !out_port) {
        return -1;
    }

    errno = 0;
    parsed = strtol(token, &endptr, 10);
    if (errno != 0 || endptr == token || *endptr != '\0' || parsed < 1 ||
        parsed > INT_MAX || !validate_port((int)parsed)) {
        return -1;
    }

    *out_port = (int)parsed;
    return 0;
}

static int parse_target_line(const char *line, wake_target_t *target,
                             int *skip_line) {
    char buffer[MAX_CONFIG_LINE_LEN];
    char normalized[18];
    char *hash;
    char *token = NULL;
    char *saveptr = NULL;
    char *tokens[5] = {0};
    int token_count = 0;
    int mac_first = 0;
    const char *name = NULL;
    const char *mac = NULL;
    const char *broadcast_ip = "255.255.255.255";
    int port = DEFAULT_PORT;

    if (!line || !target || !skip_line) {
        return -1;
    }

    memset(target, 0, sizeof(*target));
    *skip_line = 0;

    if (snprintf(buffer, sizeof(buffer), "%s", line) >= (int)sizeof(buffer)) {
        return -1;
    }

    trim_whitespace(buffer);
    if (buffer[0] == '\0' || buffer[0] == '#') {
        *skip_line = 1;
        return 0;
    }

    hash = strchr(buffer, '#');
    if (hash) {
        *hash = '\0';
        trim_whitespace(buffer);
        if (buffer[0] == '\0') {
            *skip_line = 1;
            return 0;
        }
    }

    /* Split by spaces/tabs into up to 4 fields:
     * 1) MAC
     * 2) MAC + broadcast + port
     * 3) name + MAC + optional broadcast/port
     */
    token = strtok_r(buffer, " \t", &saveptr);
    while (token && token_count < 5) {
        tokens[token_count++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }

    if (token_count == 0) {
        *skip_line = 1;
        return 0;
    }
    if (token_count > 4) {
        return -1;
    }

    /* If first token looks like a MAC, line is unnamed; otherwise token[0] is name. */
    mac_first = normalize_mac(tokens[0], normalized, sizeof(normalized));

    if (token_count == 1) {
        mac = tokens[0];
    } else if (mac_first) {
        mac = tokens[0];
        if (token_count >= 2) {
            broadcast_ip = tokens[1];
        }
        if (token_count == 3 && parse_port_string(tokens[2], &port) != 0) {
            return -1;
        }
    } else {
        name = tokens[0];
        mac = tokens[1];
        if (token_count >= 3) {
            broadcast_ip = tokens[2];
        }
        if (token_count == 4 && parse_port_string(tokens[3], &port) != 0) {
            return -1;
        }
    }

    if (!validate_ip(broadcast_ip)) {
        return -1;
    }

    if (name) {
        target->name = dup_string(name);
        if (!target->name) {
            return -1;
        }
    }

    target->mac = dup_string(mac);
    if (!target->mac) {
        free_single_target(target);
        return -1;
    }

    target->broadcast_ip = dup_string(broadcast_ip);
    if (!target->broadcast_ip) {
        free_single_target(target);
        return -1;
    }

    target->port = port;
    return 1;
}

int read_targets_from_config(target_list_t *list) {
    int path_status = 0;
    char *config_path = NULL;
    FILE *file = NULL;
    char line[MAX_CONFIG_LINE_LEN];

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

    /* Parse each non-empty config line into one wake target. */
    while (fgets(line, sizeof(line), file)) {
        wake_target_t target = {0};
        int skip_line = 0;
        int parse_result = parse_target_line(line, &target, &skip_line);
        if (parse_result < 0) {
            fclose(file);
            free_target_list(list);
            return -1;
        }
        if (skip_line) {
            continue;
        }
        if (append_target(list, &target) != 0) {
            fclose(file);
            free_single_target(&target);
            free_target_list(list);
            return -1;
        }
    }

    fclose(file);
    return (int)list->count;
}

int read_macs_from_config(mac_list_t *list) {
    target_list_t target_list = {0};
    int count = 0;

    if (!list) {
        return -1;
    }
    list->items = NULL;
    list->count = 0;

    count = read_targets_from_config(&target_list);
    if (count <= 0) {
        free_target_list(&target_list);
        return count;
    }

    for (size_t i = 0; i < target_list.count; i++) {
        if (append_mac(list, target_list.items[i].mac) != 0) {
            free_mac_list(list);
            free_target_list(&target_list);
            return -1;
        }
    }

    free_target_list(&target_list);
    return (int)list->count;
}

void free_target_list(target_list_t *list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free_single_target(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
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

    /* Take the first non-empty, non-comment line from stdin as MAC input. */
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
    target_list_t list = {0};
    char *first = NULL;
    size_t len;
    int rc = read_targets_from_config(&list);

    if (rc <= 0) {
        return NULL;
    }

    len = strlen(list.items[0].mac);
    first = malloc(len + 1);
    if (first) {
        memcpy(first, list.items[0].mac, len + 1);
    }
    free_target_list(&list);
    return first;
}
