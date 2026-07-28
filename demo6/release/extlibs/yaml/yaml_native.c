/* yaml_native.c - Simplified YAML parser/emitter */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];
static char val_buf[4096];

/* Trim leading spaces */
static const char *trim(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Find value for a key in YAML text (supports nested via dot notation) */
const char *xyaml_get(const char *yaml, const char *key) {
    const char *p = yaml;
    int klen = (int)strlen(key);

    /* Handle dot notation for nesting */
    char prefix[256] = "";
    char actual_key[256];
    strcpy(actual_key, key);

    /* Simple flat lookup: find "key:" at start of line (after optional spaces) */
    while (*p) {
        const char *line = p;
        /* Find end of line */
        const char *eol = strchr(p, '\n');
        int line_len = eol ? (int)(eol - p) : (int)strlen(p);

        /* Skip leading spaces */
        const char *content = trim(line);
        int indent = (int)(content - line);

        /* Check if line starts with key */
        if (indent == 0 && strncmp(content, actual_key, klen) == 0 && content[klen] == ':') {
            const char *val = trim(content + klen + 1);
            /* Copy value (strip trailing whitespace/comments) */
            int vlen = 0;
            while (val[vlen] && val[vlen] != '\n' && val[vlen] != '\r' && val[vlen] != '#') vlen++;
            while (vlen > 0 && (val[vlen-1] == ' ' || val[vlen-1] == '\t')) vlen--;
            memcpy(val_buf, val, vlen);
            val_buf[vlen] = '\0';
            /* Strip quotes if present */
            if (vlen >= 2 && val_buf[0] == '"' && val_buf[vlen-1] == '"') {
                memmove(val_buf, val_buf+1, vlen-2);
                val_buf[vlen-2] = '\0';
            }
            return val_buf;
        }
        p = eol ? eol + 1 : p + line_len;
        if (!eol) break;
    }
    return "";
}

/* Get integer value */
long long xyaml_get_int(const char *yaml, const char *key) {
    const char *v = xyaml_get(yaml, key);
    return atoll(v);
}

/* Get float value */
double xyaml_get_float(const char *yaml, const char *key) {
    const char *v = xyaml_get(yaml, key);
    return atof(v);
}

/* Get bool value */
long long xyaml_get_bool(const char *yaml, const char *key) {
    const char *v = xyaml_get(yaml, key);
    if (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "on") == 0 || strcmp(v, "1") == 0) return 1;
    return 0;
}

/* Check if key exists */
long long xyaml_has(const char *yaml, const char *key) {
    const char *v = xyaml_get(yaml, key);
    return v[0] != '\0' ? 1 : 0;
}

/* Count top-level keys */
long long xyaml_count_keys(const char *yaml) {
    const char *p = yaml;
    long long count = 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        /* Line starts with non-space and contains ':' */
        if (*p != ' ' && *p != '\t' && *p != '#' && *p != '\n' && *p != '-') {
            const char *colon = strchr(p, ':');
            if (colon && (!eol || colon < eol)) count++;
        }
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return count;
}

/* Get key by index (0-based) */
const char *xyaml_key_at(const char *yaml, long long index) {
    const char *p = yaml;
    long long idx = 0;
    static char key_buf[256];
    while (*p) {
        const char *eol = strchr(p, '\n');
        if (*p != ' ' && *p != '\t' && *p != '#' && *p != '\n' && *p != '-') {
            const char *colon = strchr(p, ':');
            if (colon && (!eol || colon < eol)) {
                if (idx == index) {
                    int klen = (int)(colon - p);
                    memcpy(key_buf, p, klen);
                    key_buf[klen] = '\0';
                    return key_buf;
                }
                idx++;
            }
        }
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return "";
}

/* Emit a key-value pair */
const char *xyaml_emit_pair(const char *key, const char *value) {
    sprintf(buf, "%s: %s\n", key, value);
    return buf;
}

/* Validate basic YAML syntax (returns 1 if ok) */
long long xyaml_validate(const char *yaml) {
    const char *p = yaml;
    while (*p) {
        const char *eol = strchr(p, '\n');
        if (*p != ' ' && *p != '\t' && *p != '#' && *p != '\n' && *p != '-') {
            /* Non-indented, non-comment line should have a colon or be a list item */
            const char *colon = strchr(p, ':');
            if (!colon || (eol && colon > eol)) {
                /* Check if it's a document marker */
                if (strncmp(p, "---", 3) != 0 && strncmp(p, "...", 3) != 0)
                    return 0;
            }
        }
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return 1;
}

/* Get list items under a key (returns count, stores items) */
static char list_items[64][256];
static int list_count = 0;

long long xyaml_list_count(const char *yaml, const char *key) {
    const char *p = yaml;
    int klen = (int)strlen(key);
    int found = 0;
    list_count = 0;

    while (*p) {
        const char *eol = strchr(p, '\n');
        int line_len = eol ? (int)(eol - p) : (int)strlen(p);
        const char *content = trim(p);
        int indent = (int)(content - p);

        if (!found) {
            if (indent == 0 && strncmp(content, key, klen) == 0 && content[klen] == ':') {
                found = 1;
            }
        } else {
            if (indent == 0 && *content != '-') break; /* next top-level key */
            if (*content == '-') {
                const char *item = trim(content + 1);
                int ilen = 0;
                while (item[ilen] && item[ilen] != '\n' && item[ilen] != '\r') ilen++;
                while (ilen > 0 && item[ilen-1] == ' ') ilen--;
                if (list_count < 64) {
                    memcpy(list_items[list_count], item, ilen);
                    list_items[list_count][ilen] = '\0';
                    list_count++;
                }
            }
        }
        p = eol ? eol + 1 : p + line_len;
        if (!eol) break;
    }
    return list_count;
}

/* Get list item by index (after calling list_count) */
const char *xyaml_list_at(long long index) {
    if (index < 0 || index >= list_count) return "";
    return list_items[index];
}
