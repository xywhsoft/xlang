/*
 * dotenv_native.c - .env file parser and environment variable store
 */
#include <stdio.h>
#include "../xlang_extlib_error.h"

/* xl_runtime.h 不暴露文件模块，扩展库只声明稳定的 XRT 文件入口。 */
static char dotenv_buf[4096];

#define DOTENV_MAX_VARS 512

typedef struct {
    char *key;
    char *value;
} DotenvVar;

static DotenvVar dotenv_vars[DOTENV_MAX_VARS];
static int dotenv_count = 0;

static void dotenv_error(int kind, const char *message) {
    xl6ExtlibError(kind, "xlang.extlib.dotenv", message);
}

static char *dotenv_dup(const char *text) {
    size_t size = text != NULL ? strlen(text) : 0u;
    char *result = (char *)xrtMalloc(size + 1u);
    if (result == NULL) return NULL;
    if (size != 0u) memcpy(result, text, size);
    result[size] = '\0';
    return result;
}

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

static int dotenv_set_internal(const char *key, const char *value) {
    char *existing = dotenv_find(key);
    char *copy;
    if (existing) {
        /* find index and update */
        int i;
        for (i = 0; i < dotenv_count; i++) {
            if (strcmp(dotenv_vars[i].key, key) == 0) {
                copy = dotenv_dup(value);
                if (copy == NULL) return 0;
                xrtFree(dotenv_vars[i].value);
                dotenv_vars[i].value = copy;
                return 1;
            }
        }
    }
    if (dotenv_count >= DOTENV_MAX_VARS) {
        dotenv_error(XL6_EXTERR_VALUE, "dotenv variable capacity is exhausted");
        return 0;
    }
    dotenv_vars[dotenv_count].key = dotenv_dup(key);
    dotenv_vars[dotenv_count].value = dotenv_dup(value);
    if (dotenv_vars[dotenv_count].key == NULL || dotenv_vars[dotenv_count].value == NULL) {
        xrtFree(dotenv_vars[dotenv_count].key);
        xrtFree(dotenv_vars[dotenv_count].value);
        dotenv_vars[dotenv_count].key = NULL;
        dotenv_vars[dotenv_count].value = NULL;
        return 0;
    }
    dotenv_count++;
    return 1;
}

int xdotenv_load(const char *path) {
    unsigned char *input;
    size_t input_size;
    char *text;
    char *line;
    char *next;
    int loaded = 0;
    if (!path || !path[0]) { dotenv_error(XL6_EXTERR_ARGUMENT, "dotenv path must not be empty"); return -1; }
    input = xrtFileReadAll(path, &input_size);
    if (input == NULL) { dotenv_error(XL6_EXTERR_IO, "cannot read dotenv file"); return -1; }
    text = (char *)xrtMalloc(input_size + 1u);
    if (text == NULL) { xrtFree(input); dotenv_error(XL6_EXTERR_MEMORY, "cannot allocate dotenv input"); return -1; }
    if (input_size != 0u) memcpy(text, input, input_size);
    text[input_size] = '\0';
    xrtFree(input);

    line = text;
    while (line != NULL) {
        char *eq, *key, *value;
        next = strchr(line, '\n');
        if (next != NULL) { *next = '\0'; }
        dotenv_trim(line);
        /* skip empty and comments */
        if (line[0] == '\0' || line[0] == '#') { line = next != NULL ? next + 1 : NULL; continue; }
        /* skip 'export ' prefix */
        key = line;
        if (strncmp(key, "export ", 7) == 0) key += 7;
        eq = strchr(key, '=');
        if (!eq) { line = next != NULL ? next + 1 : NULL; continue; }
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
        if (key[0] != '\0') {
            if (!dotenv_set_internal(key, value)) { xrtFree(text); dotenv_error(XL6_EXTERR_MEMORY, "cannot store dotenv variable"); return -1; }
            loaded++;
        }
        line = next != NULL ? next + 1 : NULL;
    }
    xrtFree(text);
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
    if (!key || !key[0]) { dotenv_error(XL6_EXTERR_ARGUMENT, "dotenv key must not be empty"); return; }
    if (!dotenv_set_internal(key, value ? value : "")) {
        dotenv_error(XL6_EXTERR_MEMORY, "cannot store dotenv variable");
    }
}

int xdotenv_count(void) {
    return dotenv_count;
}

void xdotenv_clear(void) {
    int i;
    for (i = 0; i < dotenv_count; i++) {
        xrtFree(dotenv_vars[i].key);
        xrtFree(dotenv_vars[i].value);
    }
    dotenv_count = 0;
}

int xdotenv_has(const char *key) {
    if (!key) return 0;
    return dotenv_find(key) != NULL;
}
