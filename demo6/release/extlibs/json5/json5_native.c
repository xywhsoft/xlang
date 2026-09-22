/*
 * json5_native.c - JSON5 parser (simplified: validate, get by path, strip to JSON)
 * Supports: comments, unquoted keys, single-quoted strings, trailing commas, hex numbers
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
static char j5_buf[8192];

/* Skip whitespace and comments */
static const char *j5_skip(const char *p) {
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { p++; continue; }
        if (p[0] == '/' && p[1] == '/') { p += 2; while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0]=='*' && p[1]=='/')) p++; if (*p) p += 2; continue; }
        break;
    }
    return p;
}

/* Check if char is valid identifier start */
static int j5_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == '$';
}
static int j5_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

/* Skip a JSON5 value, return pointer after it. NULL on error. */
static const char *j5_skip_value(const char *p);

static const char *j5_skip_string(const char *p) {
    char quote = *p++;
    while (*p && *p != quote) {
        if (*p == '\\') p++;
        p++;
    }
    if (*p == quote) return p + 1;
    return NULL;
}

static const char *j5_skip_value(const char *p) {
    p = j5_skip(p);
    if (!*p) return NULL;
    if (*p == '"' || *p == '\'') return j5_skip_string(p);
    if (*p == '{') {
        p++;
        p = j5_skip(p);
        if (*p == '}') return p + 1;
        while (*p) {
            /* key */
            p = j5_skip(p);
            if (*p == '"' || *p == '\'') { p = j5_skip_string(p); if (!p) return NULL; }
            else if (j5_ident_start(*p)) { while (j5_ident_char(*p)) p++; }
            else return NULL;
            p = j5_skip(p);
            if (*p != ':') return NULL;
            p++;
            p = j5_skip_value(p);
            if (!p) return NULL;
            p = j5_skip(p);
            if (*p == ',') { p++; p = j5_skip(p); if (*p == '}') return p + 1; continue; }
            if (*p == '}') return p + 1;
            return NULL;
        }
        return NULL;
    }
    if (*p == '[') {
        p++;
        p = j5_skip(p);
        if (*p == ']') return p + 1;
        while (*p) {
            p = j5_skip_value(p);
            if (!p) return NULL;
            p = j5_skip(p);
            if (*p == ',') { p++; p = j5_skip(p); if (*p == ']') return p + 1; continue; }
            if (*p == ']') return p + 1;
            return NULL;
        }
        return NULL;
    }
    /* number, true, false, null, Infinity, NaN */
    if (*p == '-' || *p == '+' || *p == '.' || isdigit((unsigned char)*p)) {
        if (*p == '-' || *p == '+') p++;
        if (p[0]=='0' && (p[1]=='x'||p[1]=='X')) { p += 2; while (isxdigit((unsigned char)*p)) p++; return p; }
        while (isdigit((unsigned char)*p)) p++;
        if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
        if (*p == 'e' || *p == 'E') { p++; if (*p=='+'||*p=='-') p++; while (isdigit((unsigned char)*p)) p++; }
        return p;
    }
    if (strncmp(p, "true", 4) == 0) return p + 4;
    if (strncmp(p, "false", 5) == 0) return p + 5;
    if (strncmp(p, "null", 4) == 0) return p + 4;
    if (strncmp(p, "Infinity", 8) == 0) return p + 8;
    if (strncmp(p, "NaN", 3) == 0) return p + 3;
    if (j5_ident_start(*p)) { while (j5_ident_char(*p)) p++; return p; }
    return NULL;
}

int xjson5_valid(const char *text) {
    const char *end;
    if (!text) return 0;
    end = j5_skip_value(text);
    if (!end) return 0;
    end = j5_skip(end);
    return *end == '\0';
}

/* Get value at dot-path (e.g. "a.b.0.c"). Returns raw value string. */
const char *xjson5_get(const char *text, const char *path) {
    const char *p = text;
    char key[256];
    const char *dot;
    if (!text || !path) return "";

    while (*path) {
        /* extract next path segment */
        dot = strchr(path, '.');
        int klen = dot ? (int)(dot - path) : (int)strlen(path);
        if (klen >= (int)sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, path, klen);
        key[klen] = '\0';
        path = dot ? dot + 1 : path + strlen(path);

        p = j5_skip(p);
        if (*p == '{') {
            p++;
            int found = 0;
            p = j5_skip(p);
            while (*p && *p != '}') {
                /* parse key */
                char cur_key[256];
                int ck_len = 0;
                p = j5_skip(p);
                if (*p == '"' || *p == '\'') {
                    char q = *p++;
                    while (*p && *p != q) { if (*p=='\\') p++; if (ck_len < 255) cur_key[ck_len++] = *p; p++; }
                    if (*p) p++;
                } else {
                    while (j5_ident_char(*p) && ck_len < 255) cur_key[ck_len++] = *p++;
                }
                cur_key[ck_len] = '\0';
                p = j5_skip(p);
                if (*p == ':') p++;
                p = j5_skip(p);
                if (strcmp(cur_key, key) == 0) {
                    found = 1;
                    break;
                }
                /* skip value */
                p = j5_skip_value(p);
                if (!p) return "";
                p = j5_skip(p);
                if (*p == ',') p++;
            }
            if (!found) return "";
        } else if (*p == '[') {
            int idx = atoi(key);
            int cur = 0;
            p++;
            p = j5_skip(p);
            while (*p && *p != ']') {
                if (cur == idx) break;
                p = j5_skip_value(p);
                if (!p) return "";
                p = j5_skip(p);
                if (*p == ',') { p++; p = j5_skip(p); }
                cur++;
            }
            if (cur != idx) return "";
        } else return "";
    }

    /* extract value as string */
    {
        const char *start = j5_skip(p);
        const char *end = j5_skip_value(start);
        int len;
        if (!end) return "";
        len = (int)(end - start);
        if (len >= (int)sizeof(j5_buf)) len = sizeof(j5_buf) - 1;
        memcpy(j5_buf, start, len);
        j5_buf[len] = '\0';
        /* strip quotes from string values */
        if (len >= 2 && (j5_buf[0] == '"' || j5_buf[0] == '\'') && j5_buf[len-1] == j5_buf[0]) {
            memmove(j5_buf, j5_buf + 1, len - 2);
            j5_buf[len - 2] = '\0';
        }
    }
    return j5_buf;
}
