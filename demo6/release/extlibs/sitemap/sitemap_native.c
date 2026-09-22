/*
 * sitemap_native.c - sitemap.xml / sitemap index generation and parsing,
 * plus robots.txt parsing and allowance matching.
 *
 * Sitemap parsing scans the flat <url><loc>...</loc></url> structure
 * directly and decodes the five predefined XML entities. robots.txt
 * follows Google matching semantics: the most specific (longest) rule
 * wins and allow beats disallow on ties; user agents match case
 * insensitively with a fallback to the "*" group.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XSM_DOMAIN "xlang.extlib.sitemap"
#define XSM_BUF_MAX 65536

static void xsm_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSM_DOMAIN, 0, message);
}

static xvalue *xsm_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xsm_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

static int xsm_set_str(xvalue *obj, const char *key, const char *s) {
    return xsm_set(obj, key, xsm_text(s ? s : "", s ? strlen(s) : 0));
}

/* Read a string field from an object into a malloc'd copy; NULL if absent. */
static char *xsm_read_str(xvalue *obj, const char *key) {
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj == NULL) return NULL;
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            xstrview text;
            char *copy;
            if (!xrtValueGetString(v, &text)) return NULL;
            copy = (char *)xrtMalloc(text.Size + 1);
            if (copy == NULL) return NULL;
            memcpy(copy, text.Data, text.Size);
            copy[text.Size] = '\0';
            return copy;
        }
    }
    return NULL;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XsmBuf;

static int xsm_buf_put(XsmBuf *b, const char *text) {
    size_t n = strlen(text);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        char *grown;
        while (cap < b->len + n + 1) cap *= 2;
        if (cap > XSM_BUF_MAX) return 0;
        grown = (char *)xrtMalloc(cap);
        if (grown == NULL) return 0;
        memcpy(grown, b->data, b->len);
        grown[b->len] = '\0';
        xrtFree(b->data);
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 1;
}

static int xsm_buf_escape(XsmBuf *b, const char *text) {
    const char *p;
    for (p = text; *p != '\0'; ++p) {
        switch (*p) {
            case '&': if (!xsm_buf_put(b, "&amp;")) return 0; break;
            case '<': if (!xsm_buf_put(b, "&lt;")) return 0; break;
            case '>': if (!xsm_buf_put(b, "&gt;")) return 0; break;
            case '"': if (!xsm_buf_put(b, "&quot;")) return 0; break;
            case '\'': if (!xsm_buf_put(b, "&apos;")) return 0; break;
            default: {
                char one[2] = {*p, 0};
                if (!xsm_buf_put(b, one)) return 0;
            }
        }
    }
    return 1;
}

const char *xsm_build(xvalue *urls) {
    static char ring[2][XSM_BUF_MAX];
    static int next;
    XsmBuf buf = {NULL, 0, 0};
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    if (!xsm_buf_put(&buf,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n")) {
        ring[next][0] = '\0';
        return ring[next];
    }
    n = urls != NULL ? xrtValueCount(urls) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *entry = xrtValueArrayAt(urls, i);
        char *loc = xsm_read_str(entry, "loc");
        char *lastmod = xsm_read_str(entry, "lastmod");
        char *freq = xsm_read_str(entry, "changefreq");
        char *priority = xsm_read_str(entry, "priority");
        int ok = xsm_buf_put(&buf, "  <url>\n    <loc>");
        if (ok) ok = loc != NULL && xsm_buf_escape(&buf, loc);
        if (ok) ok = xsm_buf_put(&buf, "</loc>\n");
        if (ok && lastmod != NULL && lastmod[0] != '\0') {
            ok = xsm_buf_put(&buf, "    <lastmod>") &&
                 xsm_buf_escape(&buf, lastmod) &&
                 xsm_buf_put(&buf, "</lastmod>\n");
        }
        if (ok && freq != NULL && freq[0] != '\0') {
            ok = xsm_buf_put(&buf, "    <changefreq>") &&
                 xsm_buf_escape(&buf, freq) &&
                 xsm_buf_put(&buf, "</changefreq>\n");
        }
        if (ok && priority != NULL && priority[0] != '\0') {
            ok = xsm_buf_put(&buf, "    <priority>") &&
                 xsm_buf_escape(&buf, priority) &&
                 xsm_buf_put(&buf, "</priority>\n");
        }
        if (ok) ok = xsm_buf_put(&buf, "  </url>\n");
        xrtFree(loc);
        xrtFree(lastmod);
        xrtFree(freq);
        xrtFree(priority);
        if (!ok) {
            xrtFree(buf.data);
            ring[next][0] = '\0';
            return ring[next];
        }
    }
    if (!xsm_buf_put(&buf, "</urlset>\n")) {
        xrtFree(buf.data);
        ring[next][0] = '\0';
        return ring[next];
    }
    strncpy(ring[next], buf.data ? buf.data : "", XSM_BUF_MAX - 1);
    ring[next][XSM_BUF_MAX - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
}

const char *xsm_build_index(xvalue *sitemaps) {
    static char ring[2][XSM_BUF_MAX];
    static int next;
    XsmBuf buf = {NULL, 0, 0};
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    if (!xsm_buf_put(&buf,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<sitemapindex xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n")) {
        ring[next][0] = '\0';
        return ring[next];
    }
    n = sitemaps != NULL ? xrtValueCount(sitemaps) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *entry = xrtValueArrayAt(sitemaps, i);
        char *loc = xsm_read_str(entry, "loc");
        char *lastmod = xsm_read_str(entry, "lastmod");
        int ok = xsm_buf_put(&buf, "  <sitemap>\n    <loc>");
        if (ok) ok = loc != NULL && xsm_buf_escape(&buf, loc);
        if (ok) ok = xsm_buf_put(&buf, "</loc>\n");
        if (ok && lastmod != NULL && lastmod[0] != '\0') {
            ok = xsm_buf_put(&buf, "    <lastmod>") &&
                 xsm_buf_escape(&buf, lastmod) &&
                 xsm_buf_put(&buf, "</lastmod>\n");
        }
        if (ok) ok = xsm_buf_put(&buf, "  </sitemap>\n");
        xrtFree(loc);
        xrtFree(lastmod);
        if (!ok) {
            xrtFree(buf.data);
            ring[next][0] = '\0';
            return ring[next];
        }
    }
    if (!xsm_buf_put(&buf, "</sitemapindex>\n")) {
        xrtFree(buf.data);
        ring[next][0] = '\0';
        return ring[next];
    }
    strncpy(ring[next], buf.data ? buf.data : "", XSM_BUF_MAX - 1);
    ring[next][XSM_BUF_MAX - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
}

/* Find "<tag>...</tag>" inside [start, end); returns copied text or NULL. */
static const void *xsm_memmem(const void *hay, size_t hayLen,
    const void *needle, size_t needleLen) {
    if (needleLen == 0 || hayLen < needleLen) return NULL;
    {
        const char *h = (const char *)hay;
        const char *n = (const char *)needle;
        size_t i;
        for (i = 0; i <= hayLen - needleLen; ++i) {
            if (h[i] == n[0] && memcmp(h + i, n, needleLen) == 0) {
                return h + i;
            }
        }
    }
    return NULL;
}

static char *xsm_inner(const char *start, const char *end, const char *tag) {
    char open[64];
    char close[64];
    const char *p;
    const char *closePos;
    size_t len;
    snprintf(open, sizeof(open), "<%s>", tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    p = (const char *)xsm_memmem(start, (size_t)(end - start), open, strlen(open));
    if (p == NULL) return NULL;
    p += strlen(open);
    closePos = (const char *)xsm_memmem(p, (size_t)(end - p), close, strlen(close));
    if (closePos == NULL) return NULL;
    len = (size_t)(closePos - p);
    {
        char *raw = (char *)xrtMalloc(len + 1);
        char *out;
        size_t o = 0;
        size_t i;
        if (raw == NULL) return NULL;
        memcpy(raw, p, len);
        raw[len] = '\0';
        out = (char *)xrtMalloc(len + 1);
        if (out == NULL) {
            xrtFree(raw);
            return NULL;
        }
        for (i = 0; i < len; ++i) {
            if (raw[i] == '&') {
                if (strncmp(raw + i, "&amp;", 5) == 0) { out[o++] = '&'; i += 4; }
                else if (strncmp(raw + i, "&lt;", 4) == 0) { out[o++] = '<'; i += 3; }
                else if (strncmp(raw + i, "&gt;", 4) == 0) { out[o++] = '>'; i += 3; }
                else if (strncmp(raw + i, "&quot;", 6) == 0) { out[o++] = '"'; i += 5; }
                else if (strncmp(raw + i, "&apos;", 6) == 0) { out[o++] = '\''; i += 5; }
                else out[o++] = raw[i];
            } else {
                out[o++] = raw[i];
            }
        }
        out[o] = '\0';
        xrtFree(raw);
        return out;
    }
}

static xvalue *xsm_parse_entries(const char *xml, const char *entryTag,
    const char *const *fields, int fieldCount) {
    xvalue *result = xrtValueArray();
    const char *p = xml;
    char open[64];
    char close[64];
    if (result == NULL) return NULL;
    snprintf(open, sizeof(open), "<%s>", entryTag);
    snprintf(close, sizeof(close), "</%s>", entryTag);
    while (p != NULL) {
        const char *entryStart;
        const char *entryEnd;
        p = (const char *)xsm_memmem(p, strlen(p), open, strlen(open));
        if (p == NULL) break;
        entryStart = p + strlen(open);
        entryEnd = (const char *)xsm_memmem(entryStart, strlen(entryStart),
            close, strlen(close));
        if (entryEnd == NULL) break;
        {
            xvalue *entry = xrtValueObject();
            int f;
            if (entry == NULL) {
                xrtValueRelease(result);
                return NULL;
            }
            for (f = 0; f < fieldCount; ++f) {
                char *value = xsm_inner(entryStart, entryEnd, fields[f]);
                if (value != NULL) {
                    xsm_set_str(entry, fields[f], value);
                    xrtFree(value);
                }
            }
            if (!xrtValueArrayAppendNew(result, entry)) {
                xrtValueRelease(entry);
                xrtValueRelease(result);
                return NULL;
            }
        }
        p = entryEnd + strlen(close);
    }
    return result;
}

xvalue *xsm_parse(const char *xml) {
    static const char *const fields[] = {"loc", "lastmod", "changefreq", "priority"};
    if (xml == NULL) {
        xsm_error("sitemap xml is null");
        return xrtValueNull();
    }
    if (strstr(xml, "<urlset") == NULL) {
        xsm_error("not a sitemap urlset document");
        return xrtValueNull();
    }
    return xsm_parse_entries(xml, "url", fields, 4);
}

xvalue *xsm_parse_index(const char *xml) {
    static const char *const fields[] = {"loc", "lastmod"};
    if (xml == NULL) {
        xsm_error("sitemap index xml is null");
        return xrtValueNull();
    }
    if (strstr(xml, "<sitemapindex") == NULL) {
        xsm_error("not a sitemap index document");
        return xrtValueNull();
    }
    return xsm_parse_entries(xml, "sitemap", fields, 2);
}

/* ---------------- robots.txt ---------------- */

static char *xsm_trim_copy(const char *s, const char *end) {
    while (s < end && isspace((unsigned char)*s)) s++;
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    {
        char *copy = (char *)xrtMalloc((size_t)(end - s) + 1);
        if (copy == NULL) return NULL;
        memcpy(copy, s, (size_t)(end - s));
        copy[end - s] = '\0';
        return copy;
    }
}

/* Get or create the rules object for an agent name inside agents. */
static xvalue *xsm_agent_rules(xvalue *agents, const char *name) {
    int64 i;
    int64 n = xrtValueCount(agents);
    size_t len = strlen(name);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(agents, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, name, len) == 0) {
            return v;
        }
    }
    {
        xvalue *rules = xrtValueObject();
        xvalue *allow = xrtValueArray();
        xvalue *disallow = xrtValueArray();
        if (rules == NULL || allow == NULL || disallow == NULL) {
            return NULL;
        }
        xsm_set(rules, "allow", allow);
        xsm_set(rules, "disallow", disallow);
        if (!xsm_set(agents, name, rules)) {
            return NULL;
        }
        /* the agents object now owns rules; return a borrowed view */
        return xsm_agent_rules(agents, name);
    }
}

static xvalue *xsm_rules_list(xvalue *rules, const char *key) {
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (rules == NULL) return NULL;
    n = xrtValueCount(rules);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(rules, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            return v;
        }
    }
    return NULL;
}

static void xsm_lower_copy(char *dst, size_t cap, const char *src) {
    size_t i;
    for (i = 0; i + 1 < cap && src[i] != '\0'; ++i) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

xvalue *xsm_robots_parse(const char *text) {
    xvalue *result;
    xvalue *agents;
    xvalue *sitemaps;
    const char *line;
    xvalue *currentRules = NULL;
    char lowerField[64];

    if (text == NULL) {
        xsm_error("robots text is null");
        return xrtValueNull();
    }
    result = xrtValueObject();
    agents = xrtValueObject();
    sitemaps = xrtValueArray();
    if (result == NULL || agents == NULL || sitemaps == NULL) {
        xsm_error("out of memory");
        return xrtValueNull();
    }
    line = text;
    while (line != NULL && *line != '\0') {
        const char *eol = strchr(line, '\n');
        const char *lineEnd = eol ? eol : line + strlen(line);
        char *content;
        char *colon;
        {
            const char *hash = (const char *)memchr(line, '#',
                (size_t)(lineEnd - line));
            if (hash != NULL) lineEnd = hash;
        }
        content = xsm_trim_copy(line, lineEnd);
        if (content == NULL) break;
        colon = strchr(content, ':');
        if (colon != NULL) {
            char *field = xsm_trim_copy(content, colon);
            char *value = xsm_trim_copy(colon + 1,
                colon + 1 + strlen(colon + 1));
            if (field != NULL && value != NULL) {
                xsm_lower_copy(lowerField, sizeof(lowerField), field);
                if (strcmp(lowerField, "user-agent") == 0 &&
                    value[0] != '\0') {
                    currentRules = xsm_agent_rules(agents, value);
                } else if (strcmp(lowerField, "sitemap") == 0 &&
                           value[0] != '\0') {
                    xrtValueArrayAppendNew(sitemaps,
                        xsm_text(value, strlen(value)));
                } else if (strcmp(lowerField, "allow") == 0 &&
                           currentRules != NULL) {
                    xvalue *list = xsm_rules_list(currentRules, "allow");
                    if (list != NULL) {
                        xrtValueArrayAppendNew(list,
                            xsm_text(value, strlen(value)));
                    }
                } else if (strcmp(lowerField, "disallow") == 0 &&
                           currentRules != NULL) {
                    xvalue *list = xsm_rules_list(currentRules, "disallow");
                    if (list != NULL) {
                        /* empty Disallow means allow everything */
                        if (value[0] != '\0') {
                            xrtValueArrayAppendNew(list,
                                xsm_text(value, strlen(value)));
                        }
                    }
                }
            }
            xrtFree(field);
            xrtFree(value);
        }
        xrtFree(content);
        line = eol ? eol + 1 : NULL;
    }
    xsm_set(result, "agents", agents);
    xsm_set(result, "sitemaps", sitemaps);
    return result;
}

/* Case-insensitive agent lookup with "*" fallback. */
static xvalue *xsm_find_agent(xvalue *agents, const char *userAgent) {
    int64 i;
    int64 n = xrtValueCount(agents);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(agents, i, &k);
        if (v != NULL && k.Size == 1 && k.Data[0] == '*') {
            continue;
        }
        if (v != NULL) {
            size_t j;
            int match = 1;
            if (strlen(userAgent) != k.Size) {
                match = 0;
            } else {
                for (j = 0; j < k.Size; ++j) {
                    if (tolower((unsigned char)userAgent[j]) !=
                        tolower((unsigned char)k.Data[j])) {
                        match = 0;
                        break;
                    }
                }
            }
            if (match) return v;
        }
    }
    /* substring token match as a fallback */
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(agents, i, &k);
        if (v != NULL && k.Size == 1 && k.Data[0] == '*') continue;
        if (v != NULL) {
            char lowerAgent[128];
            char lowerKey[128];
            xsm_lower_copy(lowerAgent, sizeof(lowerAgent), userAgent);
            xsm_lower_copy(lowerKey, sizeof(lowerKey), k.Data);
            if (lowerAgent[0] != '\0' && strstr(lowerAgent, lowerKey) != NULL) {
                return v;
            }
        }
    }
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(agents, i, &k);
        if (v != NULL && k.Size == 1 && k.Data[0] == '*') return v;
    }
    return NULL;
}

/* Rule match: path prefix, with optional trailing wildcard. */
static int xsm_rule_matches(const char *rule, const char *path) {
    size_t ruleLen = strlen(rule);
    if (ruleLen > 0 && rule[ruleLen - 1] == '*') {
        ruleLen--;
    }
    if (ruleLen == 0) return 1;
    return strncmp(rule, path, ruleLen) == 0;
}

static size_t xsm_rule_length(const char *rule) {
    size_t n = strlen(rule);
    if (n > 0 && rule[n - 1] == '*') n--;
    return n;
}

long long xsm_robots_allows(xvalue *robotsData, const char *userAgent,
    const char *path) {
    xvalue *agents;
    xvalue *rules;
    xvalue *allowList;
    xvalue *disallowList;
    int64 i;
    int64 n;
    const char *bestDisallow = NULL;
    const char *bestAllow = NULL;

    if (robotsData == NULL || path == NULL) return 1;
    agents = xsm_rules_list(robotsData, "agents");
    if (agents == NULL) return 1;
    rules = xsm_find_agent(agents, userAgent ? userAgent : "*");
    if (rules == NULL) return 1;
    allowList = xsm_rules_list(rules, "allow");
    disallowList = xsm_rules_list(rules, "disallow");
    if (disallowList == NULL) return 1;

    n = xrtValueCount(disallowList);
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(disallowList, i);
        xstrview text;
        if (item != NULL && xrtValueGetString(item, &text)) {
            char rule[512];
            if (text.Size < sizeof(rule)) {
                memcpy(rule, text.Data, text.Size);
                rule[text.Size] = '\0';
                if (xsm_rule_matches(rule, path)) {
                    if (bestDisallow == NULL ||
                        xsm_rule_length(rule) > strlen(bestDisallow)) {
                        static char keep[4][512];
                        static int next;
                        char *slot = keep[next];
                        next = (next + 1) % 4;
                        strcpy(slot, rule);
                        bestDisallow = slot;
                    }
                }
            }
        }
    }
    if (allowList != NULL) {
        n = xrtValueCount(allowList);
        for (i = 0; i < n; ++i) {
            xvalue *item = xrtValueArrayAt(allowList, i);
            xstrview text;
            if (item != NULL && xrtValueGetString(item, &text)) {
                char rule[512];
                if (text.Size < sizeof(rule)) {
                    memcpy(rule, text.Data, text.Size);
                    rule[text.Size] = '\0';
                    if (xsm_rule_matches(rule, path)) {
                        if (bestAllow == NULL ||
                            xsm_rule_length(rule) > strlen(bestAllow)) {
                            static char keep2[4][512];
                            static int next2;
                            char *slot = keep2[next2];
                            next2 = (next2 + 1) % 4;
                            strcpy(slot, rule);
                            bestAllow = slot;
                        }
                    }
                }
            }
        }
    }
    if (bestDisallow == NULL) return 1;
    if (bestAllow == NULL) return 0;
    if (strlen(bestDisallow) > strlen(bestAllow)) return 0;
    return 1;
}
