/*
 * vcard_native.c - vCard 3.0/4.0 parse and build.
 *
 * parse unfolds folded lines (leading space/tab), splits property
 * names from parameters and decodes the \\ \; \, escapes, and maps
 * common properties (N, FN, TEL, EMAIL, ORG, TITLE, ADR, URL, NOTE,
 * BDAY) into arrays so repeated entries survive. build re-emits a
 * vCard 3.0 text with proper folding at 74 octets.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XV_DOMAIN "xlang.extlib.vcard"

static void xv_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XV_DOMAIN, 0, message);
}

static xvalue *xv_null(void) {
    return xrtValueNull();
}

static xvalue *xv_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xv_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

static int xv_set_str(xvalue *obj, const char *key, const char *s) {
    return xv_set(obj, key, xv_text(s, strlen(s)));
}

static void xv_unescape(const char *src, size_t len, char *dst) {
    size_t i;
    size_t o = 0;
    for (i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            char c = src[i + 1];
            if (c == 'n' || c == 'N') dst[o++] = '\n';
            else if (c == '\\') dst[o++] = '\\';
            else if (c == ';') dst[o++] = ';';
            else if (c == ',') dst[o++] = ',';
            else {
                dst[o++] = src[i];
                dst[o++] = c;
            }
            i++;
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
}

static const char *xv_key_for(const char *name) {
    static const char *const known[] = {
        "FN", "N", "NICKNAME", "TEL", "EMAIL", "ORG", "TITLE", "ROLE",
        "ADR", "URL", "NOTE", "BDAY", "TZ", "GEO", "PHOTO", "LOGO",
        "VERSION", "UID", "REV", "CATEGORIES", "IMPP", "LANG"
    };
    static const char *const mapped[] = {
        "fn", "n", "nickname", "tel", "email", "org", "title", "role",
        "adr", "url", "note", "bday", "tz", "geo", "photo", "logo",
        "version", "uid", "rev", "categories", "impp", "lang"
    };
    size_t i;
    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (strcmp(name, known[i]) == 0) return mapped[i];
    }
    return NULL;
}

/* Append value to key, promoting to an array on the second entry. */
static void xv_add_value(xvalue *obj, const char *key, const char *value) {
    xvalue *existing = NULL;
    int64 i;
    int64 n = xrtValueCount(obj);
    size_t len = strlen(key);
    for (i = 0; i < n; i++) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            existing = v;
            break;
        }
    }
    if (existing == NULL) {
        xv_set_str(obj, key, value);
        return;
    }
    if (xrtValueType(existing) == XVALUE_ARRAY) {
        xrtValueArrayAppendNew(existing, xv_text(value, strlen(value)));
        return;
    }
    {
        xvalue *list = xrtValueArray();
        const char *prev = NULL;
        xstrview text;
        if (xrtValueGetString(existing, &text)) {
            prev = text.Data; /* borrowed view of the old value */
        }
        xrtValueArrayAppendNew(list,
            xv_text(prev ? prev : "", prev ? strlen(prev) : 0));
        xrtValueArrayAppendNew(list, xv_text(value, strlen(value)));
        xv_set(obj, key, list);
    }
}

xvalue *xv_parse(const char *text) {
    xvalue *result;
    const char *p;
    int inCard = 0;
    if (text == NULL) {
        xv_error("vcard text is null");
        return xv_null();
    }
    result = xrtValueObject();
    if (result == NULL) return xv_null();
    p = text;
    while (p != NULL && *p != '\0') {
        const char *eol = strchr(p, '\n');
        const char *lineEnd = eol ? eol : p + strlen(p);
        char line[4096];
        size_t len;
        /* copy the first segment before appending continuations */
        len = (size_t)(lineEnd - p);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        while (len > 0 && line[len - 1] == '\r') len--;
        line[len] = '\0';
        /* unfold continuations */
        while (eol != NULL && (eol[1] == ' ' || eol[1] == '\t')) {
            const char *next = eol + 2;
            const char *nextEol = strchr(next, '\n');
            size_t nlen = nextEol ? (size_t)(nextEol - next)
                                  : strlen(next);
            while (nlen > 0 && next[nlen - 1] == '\r') nlen--;
            if (nlen > sizeof(line) - 1 - len) {
                nlen = sizeof(line) - 1 - len;
            }
            memcpy(line + len, next, nlen);
            len += nlen;
            eol = nextEol;
        }
        line[len] = '\0';
        p = eol ? eol + 1 : NULL;
        if (line[0] == '\0') continue;
        if (strncmp(line, "BEGIN:VCARD", 11) == 0) {
            inCard = 1;
            continue;
        }
        if (strncmp(line, "END:VCARD", 9) == 0) {
            inCard = 0;
            continue;
        }
        if (!inCard) continue;
        {
            char name[64];
            const char *colon = strchr(line, ':');
            const char *semi;
            size_t nameLen;
            const char *mapped;
            char value[3072];
            if (colon == NULL) continue;
            semi = (const char *)memchr(line, ';', (size_t)(colon - line));
            nameLen = (semi ? semi : colon) - line;
            if (nameLen == 0 || nameLen >= sizeof(name)) continue;
            memcpy(name, line, nameLen);
            name[nameLen] = '\0';
            mapped = xv_key_for(name);
            if (mapped == NULL) continue;
            xv_unescape(colon + 1, strlen(colon + 1), value);
            xv_add_value(result, mapped, value);
        }
    }
    return result;
}

/* ---------------- build ---------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XvBuf;

static int xv_put(XvBuf *b, const char *text, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 1024;
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

static int xv_puts(XvBuf *b, const char *s) {
    return xv_put(b, s, strlen(s));
}

static int xv_emit_prop(XvBuf *b, const char *name, const char *value) {
    char escaped[3072];
    size_t o = 0;
    const char *p;
    int col = 0;
    if (!xv_puts(b, name) || !xv_puts(b, ":")) return 0;
    col = (int)strlen(name) + 1;
    for (p = value; *p != '\0' && o + 3 < sizeof(escaped); p++) {
        switch (*p) {
            case '\\': escaped[o++] = '\\'; escaped[o++] = '\\'; break;
            case ';':  escaped[o++] = '\\'; escaped[o++] = ';';  break;
            case ',':  escaped[o++] = '\\'; escaped[o++] = ',';  break;
            case '\n': escaped[o++] = '\\'; escaped[o++] = 'n';  break;
            default:   escaped[o++] = *p;
        }
    }
    escaped[o] = '\0';
    {
        size_t i;
        for (i = 0; i < o; i++) {
            if (col >= 74) {
                if (!xv_puts(b, "\r\n ")) return 0;
                col = 1;
            }
            {
                char one[2] = {escaped[i], 0};
                if (!xv_puts(b, one)) return 0;
            }
            col++;
        }
    }
    return xv_puts(b, "\r\n");
}

static const char *xv_read_str(xvalue *obj, const char *key) {
    static char ring[6][4096];
    static int next = 0;
    char *slot = ring[next];
    xvalue *value = NULL;
    int64 i;
    int64 n;
    size_t len = strlen(key);
    xstrview text;
    next = (next + 1) % 6;
    slot[0] = '\0';
    if (obj == NULL) return slot;
    n = xrtValueCount(obj);
    for (i = 0; i < n; i++) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            value = v;
            break;
        }
    }
    if (value != NULL && xrtValueGetString(value, &text)) {
        if (text.Size >= 4096) text.Size = 4095;
        memcpy(slot, text.Data, text.Size);
        slot[text.Size] = '\0';
    }
    return slot;
}

const char *xv_build(xvalue *card) {
    static char ring[2][16384];
    static int next;
    XvBuf buf = {NULL, 0, 0};
    static const char *const keys[] = {
        "version", "fn", "n", "nickname", "email", "tel", "org",
        "title", "role", "adr", "url", "bday", "categories", "note",
        "uid", "url", "photo", "logo", "lang", "impp", "tz", "geo", "rev"
    };
    static const char *const names[] = {
        "VERSION", "FN", "N", "NICKNAME", "EMAIL", "TEL", "ORG",
        "TITLE", "ROLE", "ADR", "URL", "BDAY", "CATEGORIES", "NOTE",
        "UID", "URL", "PHOTO", "LOGO", "LANG", "IMPP", "TZ", "GEO", "REV"
    };
    size_t i;
    next = (next + 1) % 2;
    ring[next][0] = '\0';
    if (!xv_puts(&buf, "BEGIN:VCARD\r\n")) goto fail;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const char *value = xv_read_str(card, keys[i]);
        if (value[0] != '\0') {
            if (!xv_emit_prop(&buf, names[i], value)) goto fail;
        }
    }
    if (!xv_puts(&buf, "END:VCARD\r\n")) goto fail;
    strncpy(ring[next], buf.data ? buf.data : "", sizeof(ring[next]) - 1);
    ring[next][sizeof(ring[next]) - 1] = '\0';
    xrtFree(buf.data);
    return ring[next];
fail:
    xrtFree(buf.data);
    ring[next][0] = '\0';
    return ring[next];
}
