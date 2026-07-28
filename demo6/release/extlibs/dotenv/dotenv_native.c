/*
 * dotenv_native.c - .env file parser and environment variable store
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern void xrtSetError(const char *msg);

static char dotenv_buf[4096];

#define DOTENV_MAX_VARS 512

typedef struct {
    char *key;
    char *value;
} DotenvVar;

static DotenvVar dotenv_vars[DOTENV_MAX_VARS];
static int dotenv_count = 0;

static void dotenv_trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) *end-- = '\0';
}

static char *dotenv_find(const char *key) {
    int i;
    for (i = 0; i < dotenv_count; i++) {
        if (strcmp(dotenv_vars[i].key, key) == 0) return dotenv_vars[i].value;
    }
    return NULL;
}

static void dotenv_set_internal(const char *key, const char *value) {
    char *existing = dotenv_find(key);
    if (existing) {
        free(existing);
        /* find index and update */
        int i;
        for (i = 0; i < dotenv_count; i++) {
            if (strcmp(dotenv_vars[i].key, key) == 0) {
                dotenv_vars[i].value = strdup(value);
                return;
            }
        }
    }
    if (dotenv_count >= DOTENV_MAX_VARS) return;
    dotenv_vars[dotenv_count].key = strdup(key);
    dotenv_vars[dotenv_count].value = strdup(value);
    dotenv_count++;
}

int xdotenv_load(const char *path) {
    FILE *f;
    char line[2048];
    int loaded = 0;
    if (!path || !path[0]) { xrtSetError("dotenv: empty path"); return -1; }
    f = fopen(path, "r");
    if (!f) { xrtSetError("dotenv: cannot open file"); return -1; }

    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *value;
        dotenv_trim(line);
        /* skip empty and comments */
        if (line[0] == '\0' || line[0] == '#') continue;
        /* skip 'export ' prefix */
        key = line;
        if (strncmp(key, "export ", 7) == 0) key += 7;
        eq = strchr(key, '=');
        if (!eq) continue;
        *eq = '\0';
        value = eq + 1;
        dotenv_trim(key);
        dotenv_trim(value);
        /* strip quotes */
        if (value[0] == '"' || value[0] == '\'') {
            char q = value[0];
            int len = (int)strlen(value);
            if (len >= 2 && value[len - 1] == q) {
                value[len - 1] = '\0';
                value++;
            }
        }
        if (key[0] == '\0') continue;
        dotenv_set_internal(key, value);
        loaded++;
    }
    fclose(f);
    return loaded;
}

const char *xdotenv_get(const char *key) {
    char *v;
    if (!key) return "";
    v = dotenv_find(key);
    if (!v) {
        /* fallback to system env */
        v = getenv(key);
    }
    if (!v) return "";
    strncpy(dotenv_buf, v, sizeof(dotenv_buf) - 1);
    dotenv_buf[sizeof(dotenv_buf) - 1] = '\0';
    return dotenv_buf;
}

void xdotenv_set(const char *key, const char *value) {
    if (!key || !key[0]) return;
    dotenv_set_internal(key, value ? value : "");
}

int xdotenv_count(void) {
    return dotenv_count;
}

void xdotenv_clear(void) {
    int i;
    for (i = 0; i < dotenv_count; i++) {
        free(dotenv_vars[i].key);
        free(dotenv_vars[i].value);
    }
    dotenv_count = 0;
}

int xdotenv_has(const char *key) {
    if (!key) return 0;
    return dotenv_find(key) != NULL;
}
