/* hcl_native.c - Simplified HCL parser */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];
static char val_buf[4096];

static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return s;
}

/* Find attribute value in HCL text: key = value */
const char *xhcl_get(const char *hcl, const char *key) {
    const char *p = hcl;
    int klen = (int)strlen(key);
    while (*p) {
        const char *line = p;
        const char *eol = strchr(p, '\n');
        const char *content = line;
        while (*content == ' ' || *content == '\t') content++;

        if (strncmp(content, key, klen) == 0) {
            const char *after = content + klen;
            while (*after == ' ' || *after == '\t') after++;
            if (*after == '=') {
                after++;
                while (*after == ' ' || *after == '\t') after++;
                int vlen = 0;
                if (*after == '"') {
                    after++;
                    while (after[vlen] && after[vlen] != '"') vlen++;
                } else {
                    while (after[vlen] && after[vlen] != '\n' && after[vlen] != '\r' &&
                           after[vlen] != ' ' && after[vlen] != '\t' && after[vlen] != '#') vlen++;
                }
                memcpy(val_buf, after, vlen);
                val_buf[vlen] = '\0';
                return val_buf;
            }
        }
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return "";
}

long long xhcl_get_int(const char *hcl, const char *key) {
    return atoll(xhcl_get(hcl, key));
}

double xhcl_get_float(const char *hcl, const char *key) {
    return atof(xhcl_get(hcl, key));
}

long long xhcl_get_bool(const char *hcl, const char *key) {
    const char *v = xhcl_get(hcl, key);
    return (strcmp(v, "true") == 0 || strcmp(v, "1") == 0) ? 1 : 0;
}

long long xhcl_has(const char *hcl, const char *key) {
    const char *v = xhcl_get(hcl, key);
    return v[0] != '\0' ? 1 : 0;
}

/* Count blocks of a given type */
long long xhcl_count_blocks(const char *hcl, const char *block_type) {
    const char *p = hcl;
    int tlen = (int)strlen(block_type);
    long long count = 0;
    while (*p) {
        const char *content = p;
        while (*content == ' ' || *content == '\t') content++;
        if (strncmp(content, block_type, tlen) == 0) {
            char c = content[tlen];
            if (c == ' ' || c == '\t' || c == '"') count++;
        }
        const char *eol = strchr(p, '\n');
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return count;
}

/* Get block label: block_type "label" { → returns label */
const char *xhcl_block_label(const char *hcl, const char *block_type, long long index) {
    const char *p = hcl;
    int tlen = (int)strlen(block_type);
    long long idx = 0;
    static char label_buf[256];
    while (*p) {
        const char *content = p;
        while (*content == ' ' || *content == '\t') content++;
        if (strncmp(content, block_type, tlen) == 0) {
            char c = content[tlen];
            if (c == ' ' || c == '\t' || c == '"') {
                if (idx == index) {
                    const char *q = content + tlen;
                    while (*q == ' ' || *q == '\t') q++;
                    if (*q == '"') {
                        q++;
                        int llen = 0;
                        while (q[llen] && q[llen] != '"') llen++;
                        memcpy(label_buf, q, llen);
                        label_buf[llen] = '\0';
                        return label_buf;
                    }
                    /* No quotes - read until space or { */
                    int llen = 0;
                    while (q[llen] && q[llen] != ' ' && q[llen] != '{' && q[llen] != '\n') llen++;
                    memcpy(label_buf, q, llen);
                    label_buf[llen] = '\0';
                    return label_buf;
                }
                idx++;
            }
        }
        const char *eol = strchr(p, '\n');
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return "";
}

/* Validate basic HCL syntax */
long long xhcl_validate(const char *hcl) {
    const char *p = hcl;
    int brace_depth = 0;
    while (*p) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') { brace_depth--; if (brace_depth < 0) return 0; }
        else if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        else if (*p == '/' && *(p+1) == '/') { while (*p && *p != '\n') p++; continue; }
        p++;
    }
    return brace_depth == 0 ? 1 : 0;
}

/* Count attributes (lines with = sign, not block headers) */
long long xhcl_count_attrs(const char *hcl) {
    const char *p = hcl;
    long long count = 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        const char *content = p;
        while (*content == ' ' || *content == '\t') content++;
        if (*content != '#' && *content != '/' && *content != '}' && *content != '{' && *content != '\n') {
            const char *eq = content;
            while (eq && (!eol || eq < eol) && *eq != '=') eq++;
            if (eq && (!eol || eq < eol) && *eq == '=') count++;
        }
        p = eol ? eol + 1 : p + strlen(p);
        if (!eol) break;
    }
    return count;
}
