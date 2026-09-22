/*
 * srt_native.c - SRT and WebVTT subtitle parsing and generation.
 *
 * Cue model: {index, startMs, endMs, lines: [...]}. SRT uses comma
 * milliseconds and numeric cue indexes; WebVTT uses dot milliseconds and
 * optional ids. Parse failures report an XRT error and return null;
 * individual malformed cues abort the whole parse.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XSRT_DOMAIN "xlang.extlib.srt"

static void xsrt_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSRT_DOMAIN, 0, message);
}

static xvalue *xsrt_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xsrt_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

/* "HH:MM:SS,mmm" or "HH:MM:SS.mmm"; returns -1 on mismatch. */
static long long xsrt_parse_stamp(const char *s) {
    int h;
    int m;
    int sec;
    int ms;
    char tail;
    const char *frac;
    size_t digits;
    if (s == NULL) return -1;
    if (sscanf(s, "%d:%d:%d%c%d", &h, &m, &sec, &tail, &ms) != 5) return -1;
    if (tail != ',' && tail != '.') return -1;
    if (h < 0 || m < 0 || m > 59 || sec < 0 || sec > 59 || ms < 0) return -1;
    /* scale by written digit count: 4 -> 400ms, 04 -> 40ms, 004 -> 4ms */
    frac = strpbrk(s, ",.");
    digits = frac != NULL ? strspn(frac + 1, "0123456789") : 0;
    if (digits == 1) ms *= 100;
    else if (digits == 2) ms *= 10;
    return (long long)h * 3600000 + m * 60000 + sec * 1000 + ms;
}

static void xsrt_format_stamp(long long ms, char *out, size_t cap, char sep) {
    long long h = ms / 3600000;
    long long m = (ms / 60000) % 60;
    long long s = (ms / 1000) % 60;
    long long frac = ms % 1000;
    snprintf(out, cap, "%02lld:%02lld:%02lld%c%03lld", h, m, s, sep, frac);
}

static int xsrt_contains_arrow(const char *line) {
    const char *p = strstr(line, "-->");
    return p != NULL;
}

/* Parse one timecode line: "start --> end". */
static int xsrt_parse_range(const char *line, long long *start,
    long long *end) {
    char buf[128];
    char *arrow;
    char *s1;
    char *s2;
    size_t len = strlen(line);
    if (len >= sizeof(buf)) return 0;
    strcpy(buf, line);
    arrow = strstr(buf, "-->");
    if (arrow == NULL) return 0;
    *arrow = '\0';
    s1 = buf;
    s2 = arrow + 3;
    /* strip trailing spaces/settings from s1 */
    {
        char *q = s1 + strlen(s1);
        while (q > s1 && isspace((unsigned char)*(q - 1))) *--q = '\0';
    }
    /* strip leading spaces and trailing settings from s2 */
    while (*s2 == ' ' || *s2 == '\t') s2++;
    {
        char *sp = strchr(s2, ' ');
        if (sp != NULL) *sp = '\0';
    }
    *start = xsrt_parse_stamp(s1);
    *end = xsrt_parse_stamp(s2);
    return *start >= 0 && *end >= 0 && *end >= *start;
}

static xvalue *xsrt_parse_cues(const char *text, int isVtt) {
    xvalue *result = xrtValueArray();
    const char *p = text;
    long long autoIndex = 0;
    if (result == NULL) return NULL;
    if (text == NULL) {
        xsrt_error("subtitle text is null");
        return xrtValueNull();
    }
    while (p != NULL && *p != '\0') {
        /* collect one line */
        const char *eol = strchr(p, '\n');
        const char *lineEnd = eol ? eol : p + strlen(p);
        char line[512];
        size_t len = (size_t)(lineEnd - p);
        int hasIndex = 0;
        long long index = 0;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        /* strip \r */
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' ||
               line[len - 1] == '\t')) {
            len--;
        }
        line[len] = '\0';
        if (eol != NULL) eol++;
        p = eol;
        if (line[0] == '\0') continue;
        /* skip headers and notes */
        if (strncmp(line, "WEBVTT", 6) == 0) continue;
        if (line[0] == '#') continue;
        if (strncmp(line, "NOTE", 4) == 0 &&
            (line[4] == '\0' || line[4] == ' ' || line[4] == '\t')) {
            continue;
        }
        /* optional index line before the arrow line */
        if (!xsrt_contains_arrow(line)) {
            char *endp = NULL;
            index = strtoll(line, &endp, 10);
            if (!isVtt) {
                if (endp == NULL || *endp != '\0') {
                    xsrt_error("expected cue index or timecode");
                    return xrtValueNull();
                }
                hasIndex = 1;
            } else {
                /* VTT ids are optional arbitrary strings */
                hasIndex = 1;
            }
            /* read the arrow line */
            if (p == NULL || *p == '\0') break;
            {
                const char *eol2 = strchr(p, '\n');
                const char *line2End = eol2 ? eol2 : p + strlen(p);
                size_t len2 = (size_t)(line2End - p);
                if (len2 >= sizeof(line)) len2 = sizeof(line) - 1;
                memcpy(line, p, len2);
                while (len2 > 0 && (line[len2 - 1] == '\r' ||
                       line[len2 - 1] == ' ')) {
                    len2--;
                }
                line[len2] = '\0';
                p = eol2 ? eol2 + 1 : NULL;
            }
        }
        if (!xsrt_contains_arrow(line)) {
            xsrt_error("cue is missing a timecode line");
            return xrtValueNull();
        }
        {
            long long start;
            long long end;
            xvalue *cue;
            xvalue *lines = xrtValueArray();
            if (!xsrt_parse_range(line, &start, &end)) {
                xsrt_error("invalid cue timecode");
                return xrtValueNull();
            }
            if (lines == NULL) return NULL;
            /* read text lines until blank */
            while (p != NULL && *p != '\0') {
                const char *eol3 = strchr(p, '\n');
                const char *textEnd = eol3 ? eol3 : p + strlen(p);
                size_t tlen = (size_t)(textEnd - p);
                char textLine[512];
                if (tlen >= sizeof(textLine)) tlen = sizeof(textLine) - 1;
                memcpy(textLine, p, tlen);
                while (tlen > 0 && textLine[tlen - 1] == '\r') tlen--;
                textLine[tlen] = '\0';
                if (textLine[0] == '\0') {
                    p = eol3 ? eol3 + 1 : NULL;
                    break;
                }
                xrtValueArrayAppendNew(lines,
                    xsrt_text(textLine, strlen(textLine)));
                p = eol3 ? eol3 + 1 : NULL;
            }
            cue = xrtValueObject();
            if (cue == NULL) return NULL;
            if (!hasIndex) index = ++autoIndex;
            xsrt_set(cue, "index", xrtValueInt(index));
            xsrt_set(cue, "startMs", xrtValueInt(start));
            xsrt_set(cue, "endMs", xrtValueInt(end));
            xsrt_set(cue, "lines", lines);
            if (!xrtValueArrayAppendNew(result, cue)) {
                return xrtValueNull();
            }
        }
    }
    return result;
}

xvalue *xsrt_parse(const char *text) {
    return xsrt_parse_cues(text, 0);
}

xvalue *xsrt_parse_vtt(const char *text) {
    return xsrt_parse_cues(text, 1);
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XsrtBuf;

static int xsrt_buf_put(XsrtBuf *b, const char *text) {
    size_t n = strlen(text);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 2048;
        char *grown;
        while (cap < b->len + n + 1) cap *= 2;
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

/* Read a string field from an object; returns "" when absent. */
static xvalue *xsrt_read_lines(xvalue *obj) {
    int64 i;
    int64 n;
    size_t len = strlen("lines");
    if (obj == NULL) return NULL;
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len &&
            memcmp(k.Data, "lines", len) == 0) {
            return v;
        }
    }
    return NULL;
}

static long long xsrt_read_int(xvalue *obj, const char *key) {
    int64 value = 0;
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj == NULL) return 0;
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            double d = 0;
            if (xrtValueGetFloat(v, &d)) return (long long)d;
            (void)xrtValueGetInt(v, &value);
            return value;
        }
    }
    return 0;
}

const char *xsrt_build(xvalue *cues) {
    static char ring[2][65536];
    static int next;
    XsrtBuf buf = {NULL, 0, 0};
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    n = cues != NULL ? xrtValueCount(cues) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *cue = xrtValueArrayAt(cues, i);
        char stamp[2][32];
        xvalue *lines;
        char text[32];
        int64 li;
        int64 ln;
        if (cue == NULL) continue;
        xsrt_format_stamp(xsrt_read_int(cue, "startMs"), stamp[0],
            sizeof(stamp[0]), ',');
        xsrt_format_stamp(xsrt_read_int(cue, "endMs"), stamp[1],
            sizeof(stamp[1]), ',');
        snprintf(text, sizeof(text), "%lld", (long long)(i + 1));
        xsrt_buf_put(&buf, text);
        xsrt_buf_put(&buf, "\n");
        xsrt_buf_put(&buf, stamp[0]);
        xsrt_buf_put(&buf, " --> ");
        xsrt_buf_put(&buf, stamp[1]);
        xsrt_buf_put(&buf, "\n");
        lines = xsrt_read_lines(cue);
        ln = lines != NULL ? xrtValueCount(lines) : 0;
        for (li = 0; li < ln; ++li) {
            xvalue *item = xrtValueArrayAt(lines, li);
            xstrview t;
            if (item != NULL && xrtValueGetString(item, &t)) {
                char copy[512];
                if (t.Size >= sizeof(copy)) t.Size = sizeof(copy) - 1;
                memcpy(copy, t.Data, t.Size);
                copy[t.Size] = '\0';
                xsrt_buf_put(&buf, copy);
                xsrt_buf_put(&buf, "\n");
            }
        }
        xsrt_buf_put(&buf, "\n");
    }
    strncpy(ring[next], buf.data ? buf.data : "", sizeof(ring[next]) - 1);
    ring[next][sizeof(ring[next]) - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
}

const char *xsrt_build_vtt(xvalue *cues) {
    static char ring[2][65536];
    static int next;
    XsrtBuf buf = {NULL, 0, 0};
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    xsrt_buf_put(&buf, "WEBVTT\n\n");
    n = cues != NULL ? xrtValueCount(cues) : 0;
    for (i = 0; i < n; ++i) {
        xvalue *cue = xrtValueArrayAt(cues, i);
        char stamp[2][32];
        xvalue *lines;
        int64 li;
        int64 ln;
        if (cue == NULL) continue;
        xsrt_format_stamp(xsrt_read_int(cue, "startMs"), stamp[0],
            sizeof(stamp[0]), '.');
        xsrt_format_stamp(xsrt_read_int(cue, "endMs"), stamp[1],
            sizeof(stamp[1]), '.');
        xsrt_buf_put(&buf, stamp[0]);
        xsrt_buf_put(&buf, " --> ");
        xsrt_buf_put(&buf, stamp[1]);
        xsrt_buf_put(&buf, "\n");
        lines = xsrt_read_lines(cue);
        ln = lines != NULL ? xrtValueCount(lines) : 0;
        for (li = 0; li < ln; ++li) {
            xvalue *item = xrtValueArrayAt(lines, li);
            xstrview t;
            if (item != NULL && xrtValueGetString(item, &t)) {
                char copy[512];
                if (t.Size >= sizeof(copy)) t.Size = sizeof(copy) - 1;
                memcpy(copy, t.Data, t.Size);
                copy[t.Size] = '\0';
                xsrt_buf_put(&buf, copy);
                xsrt_buf_put(&buf, "\n");
            }
        }
        xsrt_buf_put(&buf, "\n");
    }
    strncpy(ring[next], buf.data ? buf.data : "", sizeof(ring[next]) - 1);
    ring[next][sizeof(ring[next]) - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
}

const char *xsrt_format_time(long long ms) {
    static char ring[4][32];
    static int next;
    char *slot = ring[next];
    next = (next + 1) % 4;
    xsrt_format_stamp(ms, slot, 32, ',');
    return slot;
}

long long xsrt_parse_time(const char *stamp) {
    long long ms = xsrt_parse_stamp(stamp);
    if (ms < 0) {
        xsrt_error("invalid timestamp");
    }
    return ms;
}
