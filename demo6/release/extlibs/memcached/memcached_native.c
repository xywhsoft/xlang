/*
 * memcached_native.c - Memcached text-protocol codec.
 *
 * buildGet/buildSet/buildDelete/buildTouch/buildStats assemble command
 * lines (set carries bytes and expects the caller to send the data
 * block after). parseResponse reads VALUE/END/STORED/DELETED/NOT_FOUND
 * and stats key-value lines. All exchanges are plain text, so the
 * socket conversation runs in the xlang layer on stdlib tcp with
 * string send/recv directly.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XMC_DOMAIN "xlang.extlib.memcached"

static void xmc_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XMC_DOMAIN, 0, message);
}

static xvalue *xmc_null(void) {
    return xrtValueNull();
}

static xvalue *xmc_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xmc_set_str(xvalue *obj, const char *key, const char *s) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, xmc_text(s, strlen(s)));
}

static int xmc_set_int(xvalue *obj, const char *key, long long v) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, xrtValueInt((int64)v));
}

const char *xmc_build_get(xvalue *keys) {
    static char line[2048];
    size_t pos = 0;
    int64 i;
    int64 n;
    line[0] = '\0';
    if (keys == NULL || xrtValueType(keys) != XVALUE_ARRAY ||
        (n = xrtValueCount(keys)) < 1) {
        xmc_error("get requires at least one key");
        return line;
    }
    memcpy(line, "get", 3);
    pos = 3;
    for (i = 0; i < n; i++) {
        xvalue *item = xrtValueArrayAt(keys, i);
        xstrview text;
        if (item == NULL || !xrtValueGetString(item, &text) ||
            pos + text.Size + 2 >= sizeof(line)) {
            xmc_error("keys must be short strings");
            line[0] = '\0';
            return line;
        }
        line[pos++] = ' ';
        memcpy(line + pos, text.Data, text.Size);
        pos += text.Size;
    }
    line[pos++] = '\r';
    line[pos++] = '\n';
    line[pos] = '\0';
    return line;
}

const char *xmc_build_set(const char *key, long long flags, long long exptime,
    long long bytes) {
    static char line[256];
    if (key == NULL || strlen(key) > 200 || bytes < 0 || bytes > 100000000) {
        xmc_error("set parameters out of range");
        line[0] = '\0';
        return line;
    }
    snprintf(line, sizeof(line), "set %s %lld %lld %lld\r\n",
        key, flags, exptime, bytes);
    return line;
}

const char *xmc_build_add(const char *key, long long flags, long long exptime,
    long long bytes) {
    static char line[256];
    if (key == NULL || strlen(key) > 200) {
        xmc_error("add parameters out of range");
        line[0] = '\0';
        return line;
    }
    snprintf(line, sizeof(line), "add %s %lld %lld %lld\r\n",
        key, flags, exptime, bytes);
    return line;
}

const char *xmc_build_delete(const char *key) {
    static char line[256];
    if (key == NULL || strlen(key) > 200) {
        xmc_error("delete key out of range");
        line[0] = '\0';
        return line;
    }
    snprintf(line, sizeof(line), "delete %s\r\n", key);
    return line;
}

const char *xmc_build_touch(const char *key, long long exptime) {
    static char line[256];
    if (key == NULL || strlen(key) > 200) {
        xmc_error("touch key out of range");
        line[0] = '\0';
        return line;
    }
    snprintf(line, sizeof(line), "touch %s %lld\r\n", key, exptime);
    return line;
}

const char *xmc_build_stats(void) {
    return "stats\r\n";
}

const char *xmc_build_flush_all(void) {
    return "flush_all\r\n";
}

const char *xmc_build_version(void) {
    return "version\r\n";
}

xvalue *xmc_parse_response(const char *text) {
    xvalue *result;
    const char *line = text;
    xvalue *items = NULL;
    if (text == NULL) {
        xmc_error("response is null");
        return xmc_null();
    }
    result = xrtValueObject();
    if (result == NULL) return xmc_null();
    /* status words */
    if (strcmp(text, "STORED\r\n") == 0) {
        xmc_set_str(result, "status", "stored");
        return result;
    }
    if (strcmp(text, "NOT_STORED\r\n") == 0) {
        xmc_set_str(result, "status", "not_stored");
        return result;
    }
    if (strcmp(text, "DELETED\r\n") == 0) {
        xmc_set_str(result, "status", "deleted");
        return result;
    }
    if (strcmp(text, "NOT_FOUND\r\n") == 0) {
        xmc_set_str(result, "status", "not_found");
        return result;
    }
    if (strcmp(text, "TOUCHED\r\n") == 0) {
        xmc_set_str(result, "status", "touched");
        return result;
    }
    if (strcmp(text, "OK\r\n") == 0) {
        xmc_set_str(result, "status", "ok");
        return result;
    }
    if (strncmp(text, "END\r\n", 5) == 0) {
        xmc_set_str(result, "status", "end");
        return result;
    }
    if (strncmp(text, "ERROR\r\n", 7) == 0) {
        xmc_set_str(result, "status", "error");
        return result;
    }
    if (strncmp(text, "VERSION ", 8) == 0) {
        const char *nl = strchr(text + 8, '\r');
        xmc_set_str(result, "status", "version");
        {
            size_t n = nl != NULL ? (size_t)(nl - text - 8) : strlen(text + 8);
            char buf[64];
            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            memcpy(buf, text + 8, n);
            buf[n] = '\0';
            xmc_set_str(result, "version", buf);
        }
        return result;
    }
    if (strncmp(text, "SERVER_ERROR ", 13) == 0) {
        xmc_set_str(result, "status", "server_error");
        {
            const char *nl = strchr(text + 13, '\r');
            size_t n = nl != NULL ? (size_t)(nl - text - 13)
                                  : strlen(text + 13);
            char buf[256];
            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            memcpy(buf, text + 13, n);
            buf[n] = '\0';
            xmc_set_str(result, "message", buf);
        }
        return result;
    }
    /* VALUE <key> <flags> <bytes> [<cas>]\r\n<data>\r\nEND */
    if (strncmp(text, "VALUE ", 6) == 0) {
        char key[250];
        long long flags = 0;
        long long bytes = 0;
        long long cas = 0;
        int fields = sscanf(text + 6, "%249s %lld %lld %lld",
            key, &flags, &bytes, &cas);
        const char *dataStart;
        if (fields < 3) {
            xmc_error("malformed value header");
            return xmc_null();
        }
        dataStart = strstr(text + 6, "\r\n");
        if (dataStart == NULL) {
            xmc_error("value header has no data block");
            return xmc_null();
        }
        dataStart += 2;
        xmc_set_str(result, "status", "value");
        xmc_set_str(result, "key", key);
        xmc_set_int(result, "flags", flags);
        xmc_set_int(result, "bytes", bytes);
        if (fields >= 4) {
            xmc_set_int(result, "cas", cas);
        }
        {
            size_t n = (size_t)bytes;
            if (dataStart + n > text + strlen(text)) {
                n = strlen(dataStart);
            }
            xmc_set_str(result, "data", "");
            {
                /* copy raw data block without trailing CRLF/END */
                char *buf = (char *)xrtMalloc(n + 1);
                xstrview view;
                if (buf != NULL) {
                    memcpy(buf, dataStart, n);
                    buf[n] = '\0';
                    view.Data = "data";
                    view.Size = 4;
                    xrtValueObjectSetNew(result, view,
                        xmc_text(buf, strlen(buf)));
                    xrtFree(buf);
                }
            }
        }
        return result;
    }
    /* stats lines: STAT <name> <value> ... terminated by END */
    if (strncmp(text, "STAT ", 5) == 0) {
        xvalue *stats = xrtValueObject();
        xmc_set_str(result, "status", "stats");
        items = stats;
        while (line != NULL && strncmp(line, "STAT ", 5) == 0) {
            const char *eol = strchr(line, '\n');
            const char *lineEnd = eol ? eol : line + strlen(line);
            const char *p = line + 5;
            char name[128];
            size_t nameLen = 0;
            while (*p != ' ' && *p != '\r' && *p != '\0' &&
                   nameLen < sizeof(name) - 1) {
                name[nameLen++] = *p++;
            }
            name[nameLen] = '\0';
            if (*p == ' ') {
                p++;
                {
                    size_t vlen = (size_t)(lineEnd - p);
                    if (vlen > 0 && *(lineEnd - 1) == '\r') vlen--;
                    {
                        char value[512];
                        if (vlen >= sizeof(value)) vlen = sizeof(value) - 1;
                        memcpy(value, p, vlen);
                        value[vlen] = '\0';
                        xmc_set_str(items, name, value);
                    }
                }
            }
            line = eol ? eol + 1 : NULL;
        }
        {
            xstrview view;
            view.Data = "stats";
            view.Size = 5;
            xrtValueObjectSetNew(result, view, items);
        }
        return result;
    }
    xmc_error("unrecognized response");
    return xmc_null();
}
