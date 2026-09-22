/*
 * redis_native.c - RESP2 protocol codec for Redis clients.
 *
 * encode turns an argument array into a RESP array command. decode
 * parses one complete RESP value from a reply buffer: simple strings,
 * errors, integers, bulk strings (including null), and arrays
 * (including null and nested arrays). The returned var carries
 * {type, value, rest} where rest is the unparsed remainder so callers
 * can feed pipelined replies. Connections live in the xlang layer on
 * top of the stdlib tcp namespace; this file never touches sockets.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XR_DOMAIN "xlang.extlib.redis"

static void xr_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XR_DOMAIN, 0, message);
}

static xvalue *xr_null_value(void) {
    return xrtValueNull();
}

static xvalue *xr_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xr_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

/* Manual decimal parse: TCC strtod accepts 'd' exponents; keep safe. */
static int xr_parse_ll(const char *s, size_t n, long long *out) {
    long long value = 0;
    int neg = 0;
    size_t i = 0;
    if (n > 0 && (s[0] == '-' || s[0] == '+')) {
        neg = s[0] == '-';
        i = 1;
    }
    if (i >= n) return 0;
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = value * 10 + (s[i] - '0');
    }
    *out = neg ? -value : value;
    return 1;
}

const char *xr_encode(xvalue *args) {
    static char ring[2][8192];
    static int next;
    char *out = ring[next];
    size_t pos = 0;
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    out[0] = '\0';
    if (args == NULL || xrtValueType(args) != XVALUE_ARRAY) {
        xr_error("encode requires an array of string arguments");
        return out;
    }
    n = xrtValueCount(args);
    pos += (size_t)snprintf(out + pos, 24, "*%lld\r\n", (long long)n);
    for (i = 0; i < n; i++) {
        xvalue *item = xrtValueArrayAt(args, i);
        xstrview text;
        if (item == NULL || !xrtValueGetString(item, &text)) {
            xr_error("command arguments must be strings");
            out[0] = '\0';
            return out;
        }
        pos += (size_t)snprintf(out + pos, 24, "$%llu\r\n",
            (unsigned long long)text.Size);
        if (pos + text.Size + 2 >= 8192) {
            xr_error("command is too long");
            out[0] = '\0';
            return out;
        }
        memcpy(out + pos, text.Data, text.Size);
        pos += text.Size;
        out[pos++] = '\r';
        out[pos++] = '\n';
    }
    out[pos] = '\0';
    return out;
}

/* Parse one RESP value from [p, end). Returns consumed length or -1. */
static long xr_parse_value(const char *p, const char *end, xvalue **out);

static long xr_parse_line(const char *p, const char *end,
    const char **lineStart, size_t *lineLen) {
    const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
    if (nl == NULL || nl == p) return -1;
    if (nl[-1] != '\r') return -1;
    *lineStart = p;
    *lineLen = (size_t)(nl - 1 - p);
    return (long)(nl - p) + 1;
}

static long xr_parse_value(const char *p, const char *end, xvalue **out) {
    char type[16];
    const char *line;
    size_t lineLen;
    long consumed;
    if (p >= end) return -1;
    consumed = xr_parse_line(p + 1, end, &line, &lineLen);
    if (consumed < 0) return -1;
    {
        xvalue *item = xrtValueObject();
        if (item == NULL) return -1;
        if (*p == '+' || *p == '-') {
            if (*p == '-') {
                strcpy(type, "error");
            } else {
                strcpy(type, "simple");
            }
            xr_set(item, "type", xr_text(type, strlen(type)));
            xr_set(item, "value", xr_text(line, lineLen));
            *out = item;
            return consumed + 1;
        }
        if (*p == ':') {
            long long value = 0;
            if (!xr_parse_ll(line, lineLen, &value)) return -1;
            xr_set(item, "type", xr_text("integer", 7));
            xr_set(item, "value", xrtValueInt((int64)value));
            *out = item;
            return consumed + 1;
        }
        if (*p == '$') {
            long long len = 0;
            const char *payload;
            if (lineLen == 2 && memcmp(line, "-1", 2) == 0) {
                xr_set(item, "type", xr_text("null", 4));
                *out = item;
                return consumed + 1;
            }
            if (!xr_parse_ll(line, lineLen, &len) || len < 0) return -1;
            /* consumed counts from p+1 (after the type byte) */
            payload = p + 1 + consumed;
            if (end - payload < len + 2) return -1;
            if (payload[len] != '\r' || payload[len + 1] != '\n') {
                return -1;
            }
            xr_set(item, "type", xr_text("bulk", 4));
            xr_set(item, "value", xr_text(payload, (size_t)len));
            *out = item;
            return consumed + (long)len + 2 + 1;
        }
        if (*p == '*') {
            xvalue *elements;
            long long count = 0;
            long long i;
            long total = consumed + 1;
            if (lineLen == 2 && memcmp(line, "-1", 2) == 0) {
                xr_set(item, "type", xr_text("null", 4));
                *out = item;
                return total;
            }
            if (!xr_parse_ll(line, lineLen, &count) || count < 0) return -1;
            elements = xrtValueArray();
            if (elements == NULL) return -1;
            for (i = 0; i < count; i++) {
                xvalue *child = NULL;
                long used = xr_parse_value(p + total, end, &child);
                if (used < 0 || child == NULL) return -1;
                if (!xrtValueArrayAppendNew(elements, child)) return -1;
                total += used;
            }
            xr_set(item, "type", xr_text("array", 5));
            xr_set(item, "value", elements);
            *out = item;
            return total;
        }
        return -1;
    }
}

xvalue *xr_decode(const char *reply) {
    xvalue *item = NULL;
    long used;
    const char *end;
    if (reply == NULL) {
        xr_error("reply is null");
        return xr_null_value();
    }
    end = reply + strlen(reply);
    used = xr_parse_value(reply, end, &item);
    if (used < 0 || item == NULL) {
        xr_error("reply is incomplete or malformed");
        return xr_null_value();
    }
    {
        xvalue *result = xrtValueObject();
        if (result == NULL) return xr_null_value();
        xr_set(result, "type", xr_text("reply", 5));
        xr_set(result, "value", item);
        xr_set(result, "rest", xr_text(reply + used,
            (size_t)(end - reply - used)));
        return result;
    }
}

long long xr_is_complete(const char *reply) {
    xvalue *item = NULL;
    long used;
    if (reply == NULL) return 0;
    used = xr_parse_value(reply, reply + strlen(reply), &item);
    return used >= 0;
}
