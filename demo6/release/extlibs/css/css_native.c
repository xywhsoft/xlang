/* css_native.c - Simplified CSS parser */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];
static char val_buf[1024];

static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return s;
}

/* Get property value for first matching selector+property */
const char *xcss_get(const char *css, const char *selector, const char *property) {
    const char *p = css;
    int slen = (int)strlen(selector);
    int plen = (int)strlen(property);

    while (*p) {
        /* Find selector */
        const char *sel = strstr(p, selector);
        if (!sel) break;
        /* Find opening brace after selector */
        const char *brace = strchr(sel + slen, '{');
        if (!brace) break;
        /* Find closing brace */
        const char *end = strchr(brace, '}');
        if (!end) break;

        /* Search for property within braces */
        const char *q = brace + 1;
        while (q < end) {
            q = skip_ws(q);
            if (strncmp(q, property, plen) == 0) {
                const char *after = q + plen;
                after = skip_ws(after);
                if (*after == ':') {
                    after++;
                    after = skip_ws(after);
                    int vlen = 0;
                    while (after[vlen] && after[vlen] != ';' && after[vlen] != '}') vlen++;
                    while (vlen > 0 && (after[vlen-1] == ' ' || after[vlen-1] == '\t')) vlen--;
                    memcpy(val_buf, after, vlen);
                    val_buf[vlen] = '\0';
                    return val_buf;
                }
            }
            /* Skip to next property (after ; or }) */
            while (q < end && *q != ';') q++;
            if (q < end) q++;
        }
        p = end + 1;
    }
    return "";
}

/* Count rules (number of { } blocks) */
long long xcss_count_rules(const char *css) {
    long long count = 0;
    const char *p = css;
    while (*p) {
        if (*p == '{') count++;
        p++;
    }
    return count;
}

/* Count properties in a selector block */
long long xcss_count_props(const char *css, const char *selector) {
    const char *sel = strstr(css, selector);
    if (!sel) return 0;
    const char *brace = strchr(sel, '{');
    if (!brace) return 0;
    const char *end = strchr(brace, '}');
    if (!end) return 0;
    long long count = 0;
    const char *q = brace + 1;
    while (q < end) {
        q = skip_ws(q);
        if (q >= end) break;
        /* Found a property name */
        const char *colon = q;
        while (colon < end && *colon != ':') colon++;
        if (colon < end) count++;
        while (q < end && *q != ';') q++;
        if (q < end) q++;
    }
    return count;
}

/* Get selector at index (0-based) */
const char *xcss_selector_at(const char *css, long long index) {
    static char sel_buf[256];
    const char *p = css;
    long long idx = 0;
    while (*p) {
        p = skip_ws(p);
        if (!*p) break;
        /* Read selector until { */
        const char *brace = strchr(p, '{');
        if (!brace) break;
        if (idx == index) {
            int slen = (int)(brace - p);
            while (slen > 0 && (p[slen-1] == ' ' || p[slen-1] == '\t' || p[slen-1] == '\n')) slen--;
            if (slen > 255) slen = 255;
            memcpy(sel_buf, p, slen);
            sel_buf[slen] = '\0';
            return sel_buf;
        }
        idx++;
        const char *end = strchr(brace, '}');
        if (!end) break;
        p = end + 1;
    }
    return "";
}

/* Validate basic CSS syntax (balanced braces) */
long long xcss_validate(const char *css) {
    int depth = 0;
    const char *p = css;
    while (*p) {
        if (*p == '/' && *(p+1) == '*') { p += 2; while (*p && !(*p == '*' && *(p+1) == '/')) p++; if (*p) p += 2; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth < 0) return 0; }
        p++;
    }
    return depth == 0 ? 1 : 0;
}

/* Strip comments from CSS */
const char *xcss_strip_comments(const char *css) {
    static char out[8192];
    int opos = 0;
    const char *p = css;
    while (*p) {
        if (*p == '/' && *(p+1) == '*') {
            p += 2;
            while (*p && !(*p == '*' && *(p+1) == '/')) p++;
            if (*p) p += 2;
        } else {
            out[opos++] = *p++;
        }
    }
    out[opos] = '\0';
    return out;
}
