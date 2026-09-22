/*
 * shellwords_native.c - Shell-style argument splitting and quoting.
 *
 * split follows POSIX shell word rules: single quotes preserve everything,
 * double quotes preserve except $ ` \ and line continuations (kept simple
 * here: backslash escapes the next character), and unquoted backslash
 * escapes any next character. Empty results from quotes are preserved as
 * empty arguments. Unbalanced quotes report an error and return null.
 *
 * quote produces a POSIX-safe single word: wraps in single quotes and
 * escapes embedded quotes as '\'' . quoteWindows produces a double-quoted
 * Windows command line argument per MSVCRT rules.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XSW_DOMAIN "xlang.extlib.shellwords"
#define XSW_MAX_ARGS 512
#define XSW_MAX_LEN 8192

static void xsw_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSW_DOMAIN, 0, message);
}

static xvalue *xsw_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) {
        return xrtValueNull();
    }
    if (n > 0) {
        memcpy(owned, s, n);
    }
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) {
        xrtFree(owned);
    }
    return value;
}

static int xsw_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

xvalue *xsw_split(const char *line) {
    xvalue *result;
    const char *p = line;
    char buf[XSW_MAX_LEN];
    size_t pos = 0;
    int inSingle = 0;
    int inDouble = 0;
    int started = 0;

    result = xrtValueArray();
    if (result == NULL) {
        return xrtValueNull();
    }
    if (line == NULL) {
        xsw_error("split input is null");
        return xrtValueNull();
    }
    while (*p != '\0') {
        int c = (unsigned char)*p;
        if (inSingle) {
            if (c == '\'') {
                inSingle = 0;
            } else {
                if (pos < sizeof(buf)) buf[pos++] = (char)c;
            }
        } else if (inDouble) {
            if (c == '"') {
                inDouble = 0;
            } else if (c == '\\' &&
                       (p[1] == '"' || p[1] == '\\' || p[1] == '\n')) {
                if (p[1] == '\n') {
                    p++;
                } else {
                    if (pos < sizeof(buf)) buf[pos++] = p[1];
                    p++;
                }
            } else {
                if (pos < sizeof(buf)) buf[pos++] = (char)c;
            }
        } else if (xsw_is_space(c)) {
            if (started) {
                if (xrtValueCount(result) >= XSW_MAX_ARGS) {
                    xsw_error("too many arguments");
                    return xrtValueNull();
                }
                if (!xrtValueArrayAppendNew(result, xsw_text(buf, pos))) {
                    return xrtValueNull();
                }
                pos = 0;
                started = 0;
            }
        } else if (c == '\'') {
            inSingle = 1;
            started = 1;
        } else if (c == '"') {
            inDouble = 1;
            started = 1;
        } else if (c == '\\' && p[1] != '\0') {
            if (pos < sizeof(buf)) buf[pos++] = p[1];
            p++;
            started = 1;
        } else {
            if (pos < sizeof(buf)) buf[pos++] = (char)c;
            started = 1;
        }
        p++;
    }
    if (inSingle || inDouble) {
        xsw_error("unbalanced quotes");
        return xrtValueNull();
    }
    if (started) {
        if (!xrtValueArrayAppendNew(result, xsw_text(buf, pos))) {
            return xrtValueNull();
        }
    }
    if (result == NULL || xrtValueType(result) != XVALUE_ARRAY) {
        return xrtValueNull();
    }
    return result;
}

const char *xsw_quote(const char *word) {
    static char ring[4][XSW_MAX_LEN];
    static int next = 0;
    char *out = ring[next];
    size_t pos = 0;
    const char *p = word;
    next = (next + 1) % 4;
    if (word == NULL) {
        out[0] = '\0';
        return out;
    }
    /* plain safe words need no quoting */
    {
        int plain = p[0] != '\0';
        const char *q = p;
        while (*q != '\0') {
            if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                  (*q >= '0' && *q <= '9') ||
                  *q == '-' || *q == '_' || *q == '.' || *q == '/' ||
                  *q == '=' || *q == ':' || *q == ',' || *q == '+')) {
                plain = 0;
                break;
            }
            q++;
        }
        if (plain) {
            strncpy(out, p, XSW_MAX_LEN - 1);
            out[XSW_MAX_LEN - 1] = '\0';
            return out;
        }
    }
    if (pos < XSW_MAX_LEN - 1) out[pos++] = '\'';
    while (*p != '\0' && pos < XSW_MAX_LEN - 4) {
        if (*p == '\'') {
            out[pos++] = '\'';
            out[pos++] = '\\';
            out[pos++] = '\'';
            out[pos++] = '\'';
        } else {
            out[pos++] = *p;
        }
        p++;
    }
    if (pos < XSW_MAX_LEN - 1) out[pos++] = '\'';
    out[pos] = '\0';
    return out;
}

const char *xsw_quote_windows(const char *word) {
    static char ring[4][XSW_MAX_LEN];
    static int next = 0;
    char *out = ring[next];
    size_t pos = 0;
    const char *p = word;
    size_t backslashes = 0;
    int needsQuotes = 0;
    next = (next + 1) % 4;
    if (word == NULL) {
        out[0] = '\0';
        return out;
    }
    /* quoting needed for space, tab, quote, or empty */
    {
        const char *q = p;
        if (*q == '\0') {
            needsQuotes = 1;
        }
        while (*q != '\0') {
            if (*q == ' ' || *q == '\t' || *q == '"') {
                needsQuotes = 1;
                break;
            }
            q++;
        }
    }
    if (!needsQuotes) {
        strncpy(out, p, XSW_MAX_LEN - 1);
        out[XSW_MAX_LEN - 1] = '\0';
        return out;
    }
    if (pos < XSW_MAX_LEN - 1) out[pos++] = '"';
    while (*p != '\0' && pos < XSW_MAX_LEN - 4) {
        if (*p == '\\' || *p == '"') {
            backslashes = 0;
            while (*p == '\\') {
                backslashes++;
                p++;
            }
            if (*p == '"') {
                size_t i;
                for (i = 0; i < backslashes * 2 + 1 && pos < XSW_MAX_LEN - 2;
                     i++) {
                    out[pos++] = '\\';
                }
                out[pos++] = '"';
                p++;
            } else {
                size_t i;
                for (i = 0; i < backslashes && pos < XSW_MAX_LEN - 1; i++) {
                    out[pos++] = '\\';
                }
            }
        } else {
            out[pos++] = *p++;
        }
    }
    /* trailing backslashes before the closing quote double up */
    {
        size_t tail = 0;
        while (tail < pos && out[pos - 1 - tail] == '\\') {
            tail++;
        }
        if (tail > 0 && pos + tail < XSW_MAX_LEN - 1) {
            size_t i;
            for (i = 0; i < tail; i++) {
                out[pos++] = '\\';
            }
        }
    }
    if (pos < XSW_MAX_LEN - 1) out[pos++] = '"';
    out[pos] = '\0';
    return out;
}

const char *xsw_join(xvalue *words) {
    static char ring[2][XSW_MAX_LEN];
    static int next = 0;
    char *out = ring[next];
    size_t pos = 0;
    int64 i;
    int64 n;
    next = (next + 1) % 2;
    out[0] = '\0';
    if (words == NULL || xrtValueType(words) != XVALUE_ARRAY) {
        xsw_error("join requires an array of strings");
        return out;
    }
    n = xrtValueCount(words);
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(words, i);
        xstrview text;
        const char *quoted;
        if (item == NULL || !xrtValueGetString(item, &text)) {
            xsw_error("join entries must be strings");
            out[0] = '\0';
            return out;
        }
        {
            char word[XSW_MAX_LEN / 4];
            if (text.Size >= sizeof(word)) text.Size = sizeof(word) - 1;
            memcpy(word, text.Data, text.Size);
            word[text.Size] = '\0';
            quoted = xsw_quote(word);
        }
        if (i > 0 && pos < XSW_MAX_LEN - 1) {
            out[pos++] = ' ';
        }
        {
            size_t len = strlen(quoted);
            if (pos + len >= XSW_MAX_LEN) break;
            memcpy(out + pos, quoted, len);
            pos += len;
        }
    }
    out[pos] = '\0';
    return out;
}
