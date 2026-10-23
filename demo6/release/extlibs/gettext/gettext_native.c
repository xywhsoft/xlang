/*
 * gettext_native.c - PO/MO translation catalog parsing and lookup.
 *
 * 目录以单数 msgid（可带上下文）为键。MO 复数原文为
 * "singular\0plural"，译文形式存入数组。
 * Lookup falls back to the msgid itself when no translation exists.
 * MO parsing follows the GNU gettext binary layout (magic 0x950412de).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "gettext_native.h"
#include "gettext_plural.h"


#define XGT_DOMAIN "xlang.extlib.gettext"

static void xgt_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XGT_DOMAIN, 0, message);
}

static void xgt_limit_error(void) {
    xrtSetErrorInfo(XERR_RANGE, XGT_DOMAIN, 0,
        "gettext catalog exceeds 64 MiB input limit");
}

static int xgt_utf8(const char *text, size_t size) {
    if (!text || !xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xgt_error("gettext catalog text must be UTF-8");
        return 0;
    }
    return 1;
}

static xvalue *xgt_null(void) {
    return xrtValueNull();
}

static xvalue *xgt_text(const char *s, size_t n) {
    return xrtValueString((xstrview){s, n});
}

static char *xgt_copy(const char *text, size_t size) {
    char *copy;
    if (size == SIZE_MAX) return NULL;
    copy = xrtMalloc(size + 1);
    if (copy) {
        if (size > 0) memcpy(copy, text, size);
        copy[size] = '\0';
    }
    return copy;
}

static int xgt_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

/* Get a borrowed string value for a key; returns 0 when absent. */
static xvalue *xgt_get(xvalue *obj, const char *key) {
    if (obj == NULL || key == NULL || xrtValueType(obj) != XVALUE_OBJECT) return NULL;
    return xrtValueObjectGet(obj, (xstrview){ key, strlen(key) });
}

/* ---------------- PO text format ---------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} XgtStr;

static int xgt_put(XgtStr *s, const char *text, size_t n) {
    size_t needed;
    if (n > SIZE_MAX - s->len - 1) return 0;
    needed = s->len + n + 1;
    if (needed > s->cap) {
        size_t cap = s->cap ? s->cap : 128;
        char *grown;
        while (cap < needed) {
            if (cap > SIZE_MAX / 2) { cap = needed; break; }
            cap *= 2;
        }
        grown = (char *)xrtRealloc(s->data, cap);
        if (grown == NULL) return 0;
        s->data = grown;
        s->cap = cap;
    }
    memcpy(s->data + s->len, text, n);
    s->len += n;
    s->data[s->len] = '\0';
    return 1;
}

static int xgt_hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

/* Decode a PO quoted string body (without quotes) with escapes. */
static int xgt_decode_po_string(XgtStr *out, const char *p,
    const char *end) {
    while (p < end) {
        if (*p == '\\') {
            char value;
            if (p + 1 == end) return 0;
            switch (p[1]) {
                case 'n': if (!xgt_put(out, "\n", 1)) return 0; break;
                case 't': if (!xgt_put(out, "\t", 1)) return 0; break;
                case 'r': if (!xgt_put(out, "\r", 1)) return 0; break;
                case 'a': if (!xgt_put(out, "\a", 1)) return 0; break;
                case 'b': if (!xgt_put(out, "\b", 1)) return 0; break;
                case 'f': if (!xgt_put(out, "\f", 1)) return 0; break;
                case 'v': if (!xgt_put(out, "\v", 1)) return 0; break;
                case '"': if (!xgt_put(out, "\"", 1)) return 0; break;
                case '?': if (!xgt_put(out, "?", 1)) return 0; break;
                case '\\': if (!xgt_put(out, "\\", 1)) return 0; break;
                case 'x': {
                    unsigned code = 0;
                    p += 2;
                    if (p == end || xgt_hex_digit(*p) < 0) return 0;
                    while (p < end && xgt_hex_digit(*p) >= 0) {
                        unsigned digit = (unsigned)xgt_hex_digit(*p++);
                        if (code > (255u - digit) / 16u) return 0;
                        code = code * 16u + digit;
                    }
                    if (code == 0) return 0;
                    value = (char)code;
                    if (!xgt_put(out, &value, 1)) return 0;
                    continue;
                }
                default:
                    if (p[1] < '0' || p[1] > '7') return 0;
                    {
                        unsigned code = 0;
                        int i;
                        p++;
                        for (i = 0; i < 3 && p < end &&
                            *p >= '0' && *p <= '7'; i++, p++) {
                            unsigned digit = (unsigned)(*p - '0');
                            if (code > (255u - digit) / 8u) return 0;
                            code = code * 8u + digit;
                        }
                        if (code == 0) return 0;
                        value = (char)code;
                    }
                    if (!xgt_put(out, &value, 1)) return 0;
                    continue;
            }
            p += 2;
        } else {
            if (*p == '\0') return 0;
            if (!xgt_put(out, p, 1)) return 0;
            p++;
        }
    }
    return 1;
}

/* 正确跳过转义引号，并拒绝引号后的多余内容。 */
static int xgt_po_quoted(XgtStr *out, const char *p, const char *end) {
    const char *start;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p == end || *p != '"') return 0;
    start = ++p;
    while (p < end) {
        if (*p == '\\') {
            if (p + 1 == end) return 0;
            p += 2;
        } else if (*p == '"') {
            const char *close = p++;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            return p == end && xgt_decode_po_string(out, start, close);
        } else {
            p++;
        }
    }
    return 0;
}

typedef struct {
    XgtStr context;
    XgtStr id;
    XgtStr plural;
    XgtStr translation;
    int have_context;
    int have_id;
    int have_plural;
    int have_translation;
    int next_form;
    int active;
    int fuzzy;
} XgtPo;

static void xgt_po_free(XgtPo *entry) {
    xrtFree(entry->context.data);
    xrtFree(entry->id.data);
    xrtFree(entry->plural.data);
    xrtFree(entry->translation.data);
}

static void xgt_po_reset(XgtPo *entry) {
    XgtStr *parts[] = { &entry->context, &entry->id, &entry->plural,
        &entry->translation };
    size_t i;
    for (i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i) {
        parts[i]->len = 0;
        if (parts[i]->data) parts[i]->data[0] = '\0';
    }
    entry->have_context = entry->have_id = entry->have_plural = 0;
    entry->have_translation = entry->next_form = entry->active = entry->fuzzy = 0;
}

static int xgt_po_fuzzy_flag(const char *p, const char *end) {
    while (p < end) {
        const char *start;
        while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
        start = p;
        while (p < end && *p != ',' && *p != ' ' && *p != '\t') p++;
        if (p - start == 5 && memcmp(start, "fuzzy", 5) == 0) return 1;
    }
    return 0;
}

static int xgt_po_flush(xvalue *catalog, XgtPo *entry) {
    XgtStr key = {0};
    xvalue *translation;
    int ok;
    if (!entry->have_id && !entry->have_context && !entry->have_translation) return 1;
    if (!entry->have_id || !entry->have_translation) return 0;
    if (!xgt_utf8(entry->context.data ? entry->context.data : "", entry->context.len) ||
        !xgt_utf8(entry->id.data ? entry->id.data : "", entry->id.len) ||
        !xgt_utf8(entry->plural.data ? entry->plural.data : "", entry->plural.len) ||
        !xgt_utf8(entry->translation.data ? entry->translation.data : "", entry->translation.len))
        return 0;
    /* 0x04 只由本层拼接上下文键，不能来自 PO 的字段正文。 */
    if ((entry->context.len && memchr(entry->context.data, '\004', entry->context.len)) ||
        (entry->id.len && memchr(entry->id.data, '\004', entry->id.len)) ||
        (entry->plural.len && memchr(entry->plural.data, '\004', entry->plural.len))) {
        xgt_error("gettext context separator in message key");
        return 0;
    }
    if (entry->id.len > 0 && (entry->fuzzy ||
        (!entry->have_plural && entry->translation.len == 0))) {
        xgt_po_reset(entry);
        return 1;
    }
    if (entry->have_context &&
        (!xgt_put(&key, entry->context.data ? entry->context.data : "",
            entry->context.len) || !xgt_put(&key, "\x04", 1))) {
        xrtFree(key.data);
        return 0;
    }
    if (!xgt_put(&key, entry->id.data ? entry->id.data : "", entry->id.len)) {
        xrtFree(key.data);
        return 0;
    }
    if (xgt_get(catalog, key.data)) {
        xgt_error("duplicate gettext message key");
        xrtFree(key.data);
        return 0;
    }
    if (entry->have_plural) {
        size_t start = 0;
        size_t i;
        translation = xrtValueArray();
        if (!translation) { xrtFree(key.data); return 0; }
        for (i = 0; i <= entry->translation.len; ++i) {
            if (i == entry->translation.len || entry->translation.data[i] == '\0') {
                const char *part = entry->translation.data ?
                    entry->translation.data + start : "";
                xvalue *form = xgt_text(part, i - start);
                if (!form || !xrtValueArrayAppendNew(translation, form)) {
                    xrtValueRelease(translation);
                    xrtFree(key.data);
                    return 0;
                }
                start = i + 1;
            }
        }
    } else {
        translation = xgt_text(entry->translation.data ? entry->translation.data : "",
            entry->translation.len);
        if (!translation) { xrtFree(key.data); return 0; }
    }
    ok = xgt_set(catalog, key.data, translation);
    xrtFree(key.data);
    if (ok) xgt_po_reset(entry);
    return ok;
}

xvalue *xgt_parse_po(const char *text) {
    xvalue *catalog;
    XgtPo entry = {0};
    const char *line = text;
    size_t size;
    xrtClearError();
    if (!text) {
        xgt_error("po text is null");
        return xgt_null();
    }
    for (size = 0; size <= XGT_MAX_CATALOG_BYTES && text[size]; ++size) {}
    if (size > XGT_MAX_CATALOG_BYTES) {
        xgt_limit_error();
        return xgt_null();
    }
    if (!xgt_utf8(text, size)) return xgt_null();
    catalog = xrtValueObject();
    if (!catalog) return xgt_null();
    while (line && *line) {
        const char *eol = strchr(line, '\n');
        const char *lineEnd = eol ? eol : line + strlen(line);
        const char *p = line;
        while (lineEnd > p && *(lineEnd - 1) == '\r') lineEnd--;
        while (p < lineEnd && (*p == ' ' || *p == '\t')) p++;
        if (p == lineEnd) {
            if (!xgt_po_flush(catalog, &entry)) goto fail;
        } else if (*p == '#') {
            /* 标志属于下一条消息；待审核译文不进入运行时目录。 */
            if (entry.have_translation && !xgt_po_flush(catalog, &entry)) goto fail;
            if (p + 1 < lineEnd && p[1] == ',' &&
                xgt_po_fuzzy_flag(p + 2, lineEnd)) entry.fuzzy = 1;
        } else if (strncmp(p, "msgctxt ", 8) == 0) {
            if (entry.have_context || entry.have_id ||
                !xgt_po_quoted(&entry.context, p + 8, lineEnd)) goto fail;
            entry.have_context = 1;
            entry.active = 1;
        } else if (strncmp(p, "msgid_plural ", 13) == 0) {
            if (!entry.have_id || entry.have_plural || entry.have_translation ||
                !xgt_po_quoted(&entry.plural, p + 13, lineEnd)) goto fail;
            entry.have_plural = 1;
            entry.active = 3;
        } else if (strncmp(p, "msgid ", 6) == 0) {
            if (entry.have_id && !xgt_po_flush(catalog, &entry)) goto fail;
            if (!xgt_po_quoted(&entry.id, p + 6, lineEnd)) goto fail;
            entry.have_id = 1;
            entry.active = 2;
        } else if (strncmp(p, "msgstr", 6) == 0) {
            const char *q = p + 6;
            int indexed = q < lineEnd && *q == '[';
            if (!entry.have_id) goto fail;
            if (indexed) {
                int index = 0;
                q++;
                if (q == lineEnd || *q < '0' || *q > '9') goto fail;
                while (q < lineEnd && *q >= '0' && *q <= '9') {
                    index = index * 10 + (*q++ - '0');
                    if (index > 64) goto fail;
                }
                if (!entry.have_plural || q == lineEnd || *q++ != ']' ||
                    index != entry.next_form) goto fail;
                if (index > 0 && !xgt_put(&entry.translation, "\0", 1)) goto fail;
                entry.next_form++;
            } else if (entry.have_plural || entry.have_translation) {
                goto fail;
            }
            if (!xgt_po_quoted(&entry.translation, q, lineEnd)) goto fail;
            entry.have_translation = 1;
            entry.active = 4;
        } else if (*p == '"') {
            XgtStr *part = entry.active == 1 ? &entry.context :
                entry.active == 2 ? &entry.id :
                entry.active == 3 ? &entry.plural :
                entry.active == 4 ? &entry.translation : NULL;
            if (!part || !xgt_po_quoted(part, p, lineEnd)) goto fail;
        } else {
            goto fail;
        }
        line = eol ? eol + 1 : NULL;
    }
    if (!xgt_po_flush(catalog, &entry)) goto fail;
    if (!xgt_plural_validate(catalog)) {
        xgt_error("invalid Plural-Forms metadata");
        goto fail;
    }
    xgt_po_free(&entry);
    return catalog;
fail:
    if (!xrtGetError()) xgt_error("malformed po entry");
    xgt_po_free(&entry);
    xrtValueRelease(catalog);
    return xgt_null();
}

/* ---------------- MO binary format ---------------- */

static unsigned xgt_le32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static unsigned xgt_mo32(const unsigned char *p, int big_endian) {
    if (!big_endian) return xgt_le32(p);
    return (unsigned)p[3] | ((unsigned)p[2] << 8) |
           ((unsigned)p[1] << 16) | ((unsigned)p[0] << 24);
}

/* PO 与 MO 都以单数 msgid 为键，复数译文统一保存为数组。 */
static int xgt_mo_entry(xvalue *catalog, const unsigned char *original,
    size_t original_size, const unsigned char *translation, size_t translation_size) {
    const unsigned char *plural = memchr(original, 0, original_size);
    size_t key_size = plural ? (size_t)(plural - original) : original_size;
    char *key;
    xvalue *value = NULL;
    int ok;
    if (!xgt_utf8((const char *)original, original_size) ||
        !xgt_utf8((const char *)translation, translation_size)) return 0;
    /* 本 API 只表示一个单数和一个复数原文；不能丢弃第三段。 */
    if (plural && memchr(plural + 1, 0,
        original_size - key_size - 1u) != NULL) {
        xgt_error("mo original contains unsupported extra NUL parts");
        return 0;
    }
    {
        const unsigned char *separator = memchr(original, '\004', key_size);
        if ((separator && memchr(separator + 1, '\004',
                key_size - (size_t)(separator - original) - 1u)) ||
            (plural && memchr(plural + 1, '\004',
                original_size - key_size - 1u))) {
            xgt_error("mo original contains extra context separators");
            return 0;
        }
    }
    if (!plural && memchr(translation, 0, translation_size) != NULL) {
        xgt_error("singular mo translation contains NUL");
        return 0;
    }
    key = xgt_copy((const char *)original, key_size);
    if (!key) return 0;
    if (xgt_get(catalog, key)) {
        xgt_error("duplicate gettext message key");
        xrtFree(key);
        return 0;
    }
    if (plural) {
        size_t start = 0;
        size_t i;
        value = xrtValueArray();
        if (!value) { xrtFree(key); return 0; }
        for (i = 0; i <= translation_size; ++i) {
            if (i == translation_size || translation[i] == 0) {
                xvalue *form = xgt_text((const char *)translation + start, i - start);
                if (!form || !xrtValueArrayAppendNew(value, form)) {
                    xrtValueRelease(value);
                    xrtFree(key);
                    return 0;
                }
                start = i + 1;
            }
        }
    } else {
        value = xgt_text((const char *)translation, translation_size);
        if (!value) { xrtFree(key); return 0; }
    }
    ok = xgt_set(catalog, key, value);
    xrtFree(key);
    return ok;
}

static xvalue *xgt_parse_mo_bytes(const unsigned char *data, size_t size) {
    xvalue *catalog;
    unsigned num;
    unsigned origOff;
    unsigned transOff;
    unsigned i;
    unsigned magic;
    int big_endian;
    if (size > XGT_MAX_CATALOG_BYTES) {
        xgt_limit_error();
        return xgt_null();
    }
    if (!data || size < 28) {
        xgt_error("not a gettext mo file");
        return xgt_null();
    }
    magic = xgt_le32(data);
    if (magic != 0x950412DEu && magic != 0xDE120495u) {
        xgt_error("not a gettext mo file");
        return xgt_null();
    }
    big_endian = magic == 0xDE120495u;
    if ((xgt_mo32(data + 4, big_endian) >> 16) != 0) {
        xgt_error("unsupported mo revision");
        return xgt_null();
    }
    num = xgt_mo32(data + 8, big_endian);
    origOff = xgt_mo32(data + 12, big_endian);
    transOff = xgt_mo32(data + 16, big_endian);
    if ((size_t)origOff > size || (size_t)transOff > size ||
        (size_t)num > (size - origOff) / 8u ||
        (size_t)num > (size - transOff) / 8u) {
        xgt_error("mo table offsets are out of range");
        return xgt_null();
    }
    catalog = xrtValueObject();
    if (catalog == NULL) return xgt_null();
    for (i = 0; i < num; i++) {
        unsigned oLen = xgt_mo32(data + origOff + i * 8, big_endian);
        unsigned oOff = xgt_mo32(data + origOff + i * 8 + 4, big_endian);
        unsigned tLen = xgt_mo32(data + transOff + i * 8, big_endian);
        unsigned tOff = xgt_mo32(data + transOff + i * 8 + 4, big_endian);
        if ((size_t)oOff >= size || (size_t)tOff >= size ||
            (size_t)oLen >= size - oOff || (size_t)tLen >= size - tOff) {
            xgt_error("mo string offset is out of range");
            xrtValueRelease(catalog);
            return xgt_null();
        }
        if (data[oOff + oLen] != 0 || data[tOff + tLen] != 0) {
            xgt_error("mo string is missing its NUL terminator");
            xrtValueRelease(catalog);
            return xgt_null();
        }
        if (!xgt_mo_entry(catalog, data + oOff, oLen, data + tOff, tLen)) {
            xrtValueRelease(catalog);
            return xgt_null();
        }
    }
    if (!xgt_plural_validate(catalog)) {
        xgt_error("invalid Plural-Forms metadata");
        xrtValueRelease(catalog);
        return xgt_null();
    }
    return catalog;
}

xvalue *xgt_parse_mo(xvalue *bytesValue) {
    xbytesview view = {NULL, 0};
    xrtClearError();
    if (bytesValue == NULL) {
        xgt_error("parseMo requires a bytes value");
        return xgt_null();
    }
    if (xrtValueType(bytesValue) == XVALUE_BYTES &&
        xrtValueGetBytes(bytesValue, &view)) {
        return xgt_parse_mo_bytes((const unsigned char *)view.Data,
            view.Size);
    }
    xgt_error("parseMo requires a bytes value");
    return xgt_null();
}

xvalue *xgt_load_mo(const char *path) {
    size_t size = 0;
    bytes data;
    xvalue *result;
    xrtClearError();
    data = xrtFileReadAllLimit(path, XGT_MAX_CATALOG_BYTES, &size);
    if (data == NULL) {
        if (!xrtGetError())
            xrtSetErrorInfo(XERR_IO, XGT_DOMAIN, 0, "mo file cannot be read");
        return xgt_null();
    }
    result = xgt_parse_mo_bytes((const unsigned char *)data, size);
    xrtFree(data);
    return result;
}

/* ---------------- lookup ---------------- */

static char *xgt_context_key(const char *context, const char *msgid) {
    size_t left;
    size_t right;
    char *key;
    if (!context || !msgid) return NULL;
    left = strlen(context);
    right = strlen(msgid);
    if (memchr(context, '\004', left) != NULL) {
        xgt_error("gettext context separator in context");
        return NULL;
    }
    if (left > SIZE_MAX - right - 2) {
        xgt_error("context key is too long");
        return NULL;
    }
    key = xrtMalloc(left + right + 2);
    if (!key) {
        if (!xrtGetError())
            xrtSetErrorInfo(XERR_MEMORY, XGT_DOMAIN, 0, "out of memory");
        return NULL;
    }
    memcpy(key, context, left);
    key[left] = '\x04';
    memcpy(key + left + 1, msgid, right + 1);
    return key;
}

static void *xgt_translate(xvalue *catalog, const char *key,
    const char *singular, const char *plural, int64_t n, int use_plural) {
    xvalue *value;
    xstrview text;
    int was_array;
    size_t index = 0;
    xrtClearError();
    if (catalog == NULL || xrtValueType(catalog) != XVALUE_OBJECT) {
        xrtSetErrorInfo(XERR_TYPE, XGT_DOMAIN, 0,
            "gettext catalog must be an object");
        return NULL;
    }
    if ((singular && strchr(singular, '\004')) ||
        (plural && strchr(plural, '\004'))) {
        xgt_error("gettext context separator in message ID");
        return NULL;
    }
    value = xgt_get(catalog, key);
    was_array = value != NULL && xrtValueType(value) == XVALUE_ARRAY;
    if (use_plural && !xgt_plural_index(catalog, n, &index)) {
        xgt_error("invalid plural count or Plural-Forms metadata");
        return NULL;
    }
    if (was_array) {
        xvalue *item = xrtValueCount(value) > (size_t)index ?
            xrtValueArrayAt(value, index) : NULL;
        value = item;
    }
    if (value != NULL && (!use_plural || n == 1 || was_array) &&
        xrtValueType(value) == XVALUE_STRING && xrtValueGetString(value, &text) &&
        text.Size != 0) {
        return xgt_copy(text.Data, text.Size);
    }
    if (use_plural && n != 1 && plural) return xgt_copy(plural, strlen(plural));
    return xgt_copy(singular ? singular : "", singular ? strlen(singular) : 0);
}

void *xgt_t(xvalue *catalog, const char *msgid) {
    return xgt_translate(catalog, msgid, msgid, NULL, 1, 0);
}

void *xgt_tn(xvalue *catalog, const char *msgid,
    const char *msgidPlural, int64_t n) {
    return xgt_translate(catalog, msgid, msgid, msgidPlural, n, 1);
}

void *xgt_tc(xvalue *catalog, const char *context, const char *msgid) {
    char *key;
    void *result;
    xrtClearError();
    key = xgt_context_key(context, msgid);
    if (!key) {
        if (context && msgid) return NULL;
        return xgt_copy(msgid ? msgid : "", msgid ? strlen(msgid) : 0);
    }
    result = xgt_translate(catalog, key, msgid, NULL, 1, 0);
    xrtFree(key);
    return result;
}

void *xgt_tnc(xvalue *catalog, const char *context, const char *msgid,
    const char *msgidPlural, int64_t n) {
    char *key;
    void *result;
    xrtClearError();
    if (n < 0) {
        xgt_error("plural count must not be negative");
        return NULL;
    }
    key = xgt_context_key(context, msgid);
    if (!key) {
        if (context && msgid) return NULL;
        const char *fallback = n == 1 || !msgidPlural ?
            (msgid ? msgid : "") : msgidPlural;
        return xgt_copy(fallback, strlen(fallback));
    }
    result = xgt_translate(catalog, key, msgid, msgidPlural, n, 1);
    xrtFree(key);
    return result;
}

