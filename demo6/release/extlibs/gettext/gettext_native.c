/*
 * gettext_native.c - PO/MO translation catalog parsing and lookup.
 *
 * Catalogs map msgid -> msgstr; plural entries are stored as msgid with
 * an embedded \0 ("singular\0plural") and an array of translations.
 * Lookup falls back to the msgid itself when no translation exists.
 * MO parsing follows the GNU gettext binary layout (magic 0x950412de).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "xl_runtime.h"


#define XGT_DOMAIN "xlang.extlib.gettext"

static void xgt_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XGT_DOMAIN, 0, message);
}

static xvalue *xgt_null(void) {
    return xrtValueNull();
}

static xvalue *xgt_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xgt_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

/* Get a borrowed string value for a key; returns 0 when absent. */
static xvalue *xgt_get(xvalue *obj, const char *key) {
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj == NULL) return NULL;
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            return v;
        }
    }
    return NULL;
}

/* ---------------- PO text format ---------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XgtStr;

static int xgt_put(XgtStr *s, const char *text, size_t n) {
    if (s->len + n + 1 > s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 128;
        char *grown;
        while (cap < s->len + n + 1) cap *= 2;
        grown = (char *)xrtMalloc(cap);
        if (grown == NULL) return 0;
        memcpy(grown, s->data, s->len);
        grown[s->len] = '\0';
        xrtFree(s->data);
        s->data = grown;
        s->cap = cap;
    }
    memcpy(s->data + s->len, text, n);
    s->len += n;
    s->data[s->len] = '\0';
    return 1;
}

/* Decode a PO quoted string body (without quotes) with escapes. */
static int xgt_decode_po_string(XgtStr *out, const char *p,
    const char *end) {
    while (p < end) {
        if (*p == '\\' && p + 1 < end) {
            switch (p[1]) {
                case 'n': if (!xgt_put(out, "\n", 1)) return 0; break;
                case 't': if (!xgt_put(out, "\t", 1)) return 0; break;
                case 'r': if (!xgt_put(out, "\r", 1)) return 0; break;
                case '"': if (!xgt_put(out, "\"", 1)) return 0; break;
                case '\\': if (!xgt_put(out, "\\", 1)) return 0; break;
                default:
                    if (!xgt_put(out, p + 1, 1)) return 0;
            }
            p += 2;
        } else {
            if (!xgt_put(out, p, 1)) return 0;
            p++;
        }
    }
    return 1;
}

xvalue *xgt_parse_po(const char *text) {
    xvalue *catalog;
    const char *line = text;
    XgtStr msgid = {NULL, 0, 0};
    XgtStr msgstr = {NULL, 0, 0};
    int haveId = 0;
    int haveStr = 0;
    int havePluralId = 0;
    int lastWasId = 0; /* 1 after msgid, 2 after msgstr */
    if (text == NULL) {
        xgt_error("po text is null");
        return xgt_null();
    }
    catalog = xrtValueObject();
    if (catalog == NULL) return xgt_null();
#define XGT_FLUSH() do { \
    if (haveId && haveStr) { \
        if (havePluralId) { \
            xvalue *forms = xrtValueArray(); \
            size_t start = 0; \
            size_t k; \
            for (k = 0; k <= msgstr.len; k++) { \
                if (k == msgstr.len || msgstr.data[k] == '\0') { \
                    size_t n = k - start; \
                    xrtValueArrayAppendNew(forms, \
                        xgt_text(msgstr.data + start, n)); \
                    start = k + 1; \
                } \
            } \
            xgt_set(catalog, msgid.data ? msgid.data : "", forms); \
        } else { \
            xgt_set(catalog, msgid.data ? msgid.data : "", \
                xgt_text(msgstr.data ? msgstr.data : "", msgstr.len)); \
        } \
    } \
    msgid.len = 0; msgstr.len = 0; \
    if (msgid.data != NULL) msgid.data[0] = '\0'; \
    if (msgstr.data != NULL) msgstr.data[0] = '\0'; \
    haveId = 0; haveStr = 0; havePluralId = 0; \
} while (0)
    while (line != NULL && *line != '\0') {
        const char *eol = strchr(line, '\n');
        const char *lineEnd = eol ? eol : line + strlen(line);
        const char *p = line;
        /* strip \r and leading blanks */
        while (lineEnd > p && *(lineEnd - 1) == '\r') lineEnd--;
        while (p < lineEnd && (*p == ' ' || *p == '\t')) p++;
        if (p < lineEnd && *p != '#' && *p != '\0') {
            if (strncmp(p, "msgid_plural ", 13) == 0) {
                havePluralId = 1;
                lastWasId = 3; /* continuations extend nothing */
            } else if (strncmp(p, "msgid ", 6) == 0) {
                if (haveId && haveStr) XGT_FLUSH();
                {
                    const char *q = p + 6;
                    while (q < lineEnd && *q == ' ') q++;
                    if (q < lineEnd && *q == '"') {
                        const char *start = q + 1;
                        const char *close = memchr(start, '"',
                            (size_t)(lineEnd - start));
                        if (close != NULL) {
                            msgid.len = 0;
                            xgt_decode_po_string(&msgid, start, close);
                            haveId = 1;
                            lastWasId = 1;
                        }
                    }
                }
            } else if (strncmp(p, "msgstr", 6) == 0) {
                /* covers both "msgstr " and "msgstr[N] " */
                const char *q = p + 6;
                int isPluralForm = 0;
                if (q < lineEnd && *q == '[') {
                    isPluralForm = 1;
                    while (q < lineEnd && *q != ' ') q++;
                }
                while (q < lineEnd && *q == ' ') q++;
                if (q < lineEnd && *q == '"') {
                    const char *start = q + 1;
                    const char *close = memchr(start, '"',
                        (size_t)(lineEnd - start));
                    if (close != NULL) {
                        if (isPluralForm && msgstr.len > 0) {
                            xgt_put(&msgstr, "\0", 1);
                        } else if (isPluralForm || !haveStr) {
                            msgstr.len = 0;
                        }
                        xgt_decode_po_string(&msgstr, start, close);
                        haveStr = 1;
                        lastWasId = 2;
                    }
                }
            } else if (*p == '"') {
                const char *start = p + 1;
                const char *close = memchr(start, '"',
                    (size_t)(lineEnd - start));
                if (close != NULL) {
                    if (lastWasId == 1 && haveId) {
                        xgt_decode_po_string(&msgid, start, close);
                    } else if (lastWasId == 2 && haveStr) {
                        xgt_decode_po_string(&msgstr, start, close);
                    }
                }
            } else {
                /* unknown directive such as msgctxt: flush partial state */
                XGT_FLUSH();
                lastWasId = 0;
            }
        }
        line = eol ? eol + 1 : NULL;
    }
    XGT_FLUSH();
#undef XGT_FLUSH
    xrtFree(msgid.data);
    xrtFree(msgstr.data);
    return catalog;
}

/* ---------------- MO binary format ---------------- */

static unsigned xgt_le32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static xvalue *xgt_parse_mo_bytes(const unsigned char *data, size_t size) {
    xvalue *catalog;
    unsigned num;
    unsigned origOff;
    unsigned transOff;
    unsigned i;
    if (size < 28 || xgt_le32(data) != 0x950412DEu) {
        xgt_error("not a gettext mo file");
        return xgt_null();
    }
    num = xgt_le32(data + 8);
    origOff = xgt_le32(data + 12);
    transOff = xgt_le32(data + 16);
    if ((size_t)origOff + 8u * num > size ||
        (size_t)transOff + 8u * num > size) {
        xgt_error("mo table offsets are out of range");
        return xgt_null();
    }
    catalog = xrtValueObject();
    if (catalog == NULL) return xgt_null();
    for (i = 0; i < num; i++) {
        unsigned oLen = xgt_le32(data + origOff + i * 8);
        unsigned oOff = xgt_le32(data + origOff + i * 8 + 4);
        unsigned tLen = xgt_le32(data + transOff + i * 8);
        unsigned tOff = xgt_le32(data + transOff + i * 8 + 4);
        if ((size_t)oOff + oLen > size || (size_t)tOff + tLen > size) {
            continue;
        }
        if (oLen == 0) {
            /* the header entry carries metadata, not a translation */
            continue;
        }
        {
            char key[2048];
            size_t keyLen = oLen < sizeof(key) - 1 ? oLen : sizeof(key) - 1;
            char cleaned[2048];
            size_t cleanLen = 0;
            size_t k;
            memcpy(key, data + oOff, keyLen);
            key[keyLen] = '\0';
            /* keep only the singular part before the NUL separator */
            for (k = 0; k < keyLen; k++) {
                if (key[k] == '\0') break;
                cleaned[cleanLen++] = key[k];
            }
            cleaned[cleanLen] = '\0';
            {
                char value[2048];
                size_t valueLen = tLen < sizeof(value) - 1
                    ? tLen : sizeof(value) - 1;
                memcpy(value, data + tOff, valueLen);
                value[valueLen] = '\0';
                xgt_set(catalog, cleaned, xgt_text(value, valueLen));
            }
        }
    }
    return catalog;
}

xvalue *xgt_parse_mo(xvalue *bytesValue) {
    xbytesview view = {NULL, 0};
    if (bytesValue == NULL) {
        xgt_error("parseMo requires a bytes value");
        return xgt_null();
    }
    if (xrtValueGetBytes(bytesValue, &view)) {
        return xgt_parse_mo_bytes((const unsigned char *)view.Data,
            view.Size);
    }
    /* stdlib Buffer crosses the ABI as a handle wrapping an xbuffer */
    {
        ptr handle = NULL;
        if (xrtValueGetHandle(bytesValue, &handle, NULL, NULL) &&
            handle != NULL) {
            view = xrtBufferView((const xbuffer *)handle);
            return xgt_parse_mo_bytes((const unsigned char *)view.Data,
                view.Size);
        }
    }
    xgt_error("parseMo requires a bytes value");
    return xgt_null();
}

xvalue *xgt_load_mo(const char *path) {
    size_t size = 0;
    bytes data = xrtFileReadAll(path, &size);
    xvalue *result;
    if (data == NULL) {
        xgt_error("mo file cannot be read");
        return xgt_null();
    }
    result = xgt_parse_mo_bytes((const unsigned char *)data, size);
    xrtFree(data);
    return result;
}

/* ---------------- lookup ---------------- */

const char *xgt_t(xvalue *catalog, const char *msgid) {
    xvalue *value;
    xstrview text;
    static char ring[4][2048];
    static int next = 0;
    char *slot = ring[next];
    next = (next + 1) % 4;
    slot[0] = '\0';
    if (msgid == NULL) return slot;
    value = xgt_get(catalog, msgid);
    if (value != NULL && xrtValueGetString(value, &text)) {
        if (text.Size >= 2048) text.Size = 2047;
        memcpy(slot, text.Data, text.Size);
        slot[text.Size] = '\0';
        return slot;
    }
    strncpy(slot, msgid, 2047);
    slot[2047] = '\0';
    return slot;
}

const char *xgt_tn(xvalue *catalog, const char *msgid,
    const char *msgidPlural, long long n) {
    xvalue *value = xgt_get(catalog, msgid);
    static char ring[4][2048];
    static int next = 0;
    char *slot = ring[next];
    next = (next + 1) % 4;
    slot[0] = '\0';
    if (value != NULL && xrtValueType(value) == XVALUE_ARRAY) {
        /* plural entry: [singular, plural1, ...] */
        int64 index = n == 1 ? 0 : 1;
        xvalue *item = xrtValueArrayAt(value, index);
        xstrview text;
        if (item != NULL && xrtValueGetString(item, &text)) {
            if (text.Size >= 2048) text.Size = 2047;
            memcpy(slot, text.Data, text.Size);
            slot[text.Size] = '\0';
            return slot;
        }
    }
    if (value != NULL) {
        xstrview text;
        if (xrtValueGetString(value, &text)) {
            if (text.Size >= 2048) text.Size = 2047;
            memcpy(slot, text.Data, text.Size);
            slot[text.Size] = '\0';
            return slot;
        }
    }
    /* fall back to the source forms */
    if (n == 1) {
        strncpy(slot, msgid ? msgid : "", 2047);
    } else {
        strncpy(slot, msgidPlural ? msgidPlural : (msgid ? msgid : ""), 2047);
    }
    slot[2047] = '\0';
    return slot;
}

