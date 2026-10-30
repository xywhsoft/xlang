/*
 * bson_native.c - BSON <-> xlang var bidirectional codec
 * Based on cloudwu/lua-bson (MIT License) - buffer management & primitives
 * Architecture: BSON binary <-> xvalue* (table/list/primitives) direct conversion
 */
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#include "bson_native.h"

#include <string.h>
#include <stdio.h>



static void bson_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.bson", 0, message);
}

static void bson_range_error(const char *message) {
    xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.bson", 0, message);
}

/* 分配器已有更精确的内存诊断时，不用格式错误覆盖它。 */
static void bson_memory_error(void) {
    if (xrtGetError() == NULL) {
        xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.bson", 0,
            "bson: out of memory");
    }
}

/*
 * BSON needs only a small compatibility vocabulary for var values.  Keep it
 * local: extensions must use the published XRT value API rather than revive
 * the old xvo ABI as a second runtime contract.
 */
static xvalue *bson_null(void) {
    return xrtValueRetain(xrtValueNull());
}

static bool xvoIsNull(const xvalue *value) {
    return value == NULL || xrtValueType(value) == XVALUE_NULL;
}

static bool xvoIsBool(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_BOOL;
}

static bool xvoIsInt(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_INT;
}

static bool xvoIsFloat(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_FLOAT;
}

static bool xvoIsText(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_STRING;
}

static bool xvoIsBytes(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_BYTES;
}

static bool xvoIsTime(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_TIME;
}

static bool xvoIsArray(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_ARRAY;
}

static bool xvoIsList(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_INT_MAP;
}

static bool xvoIsTable(const xvalue *value) {
    return value != NULL && xrtValueType(value) == XVALUE_OBJECT;
}

static bool xvoGetBool(const xvalue *value) {
    bool result = false;
    (void)xrtValueGetBool(value, &result);
    return result;
}

static int64 xvoGetInt(const xvalue *value) {
    int64 result = 0;
    (void)xrtValueGetInt(value, &result);
    return result;
}

static double xvoGetFloat(const xvalue *value) {
    double result = 0.0;
    (void)xrtValueGetFloat(value, &result);
    return result;
}

static xtime xvoGetTime(const xvalue *value) {
    xtime result = 0;
    (void)xrtValueGetTime(value, &result);
    return result;
}

static xvalue *bson_string(const char *data, size_t size) {
    return xrtValueString((xstrview){ data != NULL ? data : "", size });
}

static xvalue *xvoCreateNull(void) {
    return bson_null();
}

static xvalue *bson_bytes_take(void *data, size_t size) {
    bytes owned = (bytes)data;
    xvalue *value = xrtValueBytesTake(&owned, size);
    xrtFree(owned);
    return value;
}

static bool bson_wire(const xvalue *value, xbytesview *result) {
    return xvoIsBytes(value) && xrtValueGetBytes(value, result);
}

static xvalue *xvoCreateArray(void) {
    return xrtValueArray();
}

static xvalue *xvoCreateTable(void) {
    return xrtValueObject();
}

static void xvoUnref(xvalue *value) {
    if (value != NULL) {
        xrtValueRelease(value);
    }
}

static uint32 xvoArrayItemCount(const xvalue *array) {
    size_t count = xrtValueCount(array);
    return count <= UINT32_MAX ? (uint32)count : UINT32_MAX;
}

static xvalue *xvoArrayGetValue(const xvalue *array, uint32 index) {
    xvalue *value = xrtValueArrayGet(array, (size_t)index);
    return value != NULL ? xrtValueRetain(value) : NULL;
}

static uint32 xvoListItemCount(const xvalue *list) {
    size_t count = xrtValueCount(list);
    return count <= UINT32_MAX ? (uint32)count : UINT32_MAX;
}

static xvalue *xvoListGetValue(const xvalue *list, int64 index) {
    xvalue *value = xrtValueIntMapGet(list, index);
    return value != NULL ? xrtValueRetain(value) : NULL;
}

/* ─── BSON type constants ─── */
#define BSON_REAL      1
#define BSON_STRING    2
#define BSON_DOCUMENT  3
#define BSON_ARRAY     4
#define BSON_BINARY    5
#define BSON_UNDEFINED 6
#define BSON_OBJECTID  7
#define BSON_BOOLEAN   8
#define BSON_DATE      9
#define BSON_NULL      10
#define BSON_REGEX     11
#define BSON_DBPOINTER 12
#define BSON_JSCODE    13
#define BSON_SYMBOL    14
#define BSON_CODEWS    15
#define BSON_INT32     16
#define BSON_TIMESTAMP 17
#define BSON_INT64     18
#define BSON_MINKEY    255
#define BSON_MAXKEY    127

#define DEFAULT_CAP 256
#define MAX_DEPTH   64
#define MAX_BSON_BYTES (64 * 1024 * 1024)
#define MAX_BSON_ITEMS 1000000u

/* ─── Dynamic buffer (from lua-bson) ─── */
struct bson {
    int size;
    int cap;
    bool failed;
    size_t items;
    uint8_t *ptr;
    uint8_t buffer[DEFAULT_CAP];
};

struct bson_reader {
    const uint8_t *ptr;
    int size;
    int depth;
    bool failed;
    size_t *items;
};

static void bson_init(struct bson *b) {
    b->size = 0;
    b->cap = DEFAULT_CAP;
    b->failed = false;
    b->items = 0u;
    b->ptr = b->buffer;
}

static void bson_free(struct bson *b) {
    if (b->ptr != b->buffer) xrtFree(b->ptr);
    b->ptr = NULL;
    b->size = 0;
    b->cap = 0;
}

static bool bson_reserve(struct bson *b, int sz) {
    int next_cap;

    if (b->failed) return false;
    if (sz < 0 || sz > MAX_BSON_BYTES - b->size) {
        bson_range_error("bson: document exceeds the 64 MiB safety limit");
        b->failed = true;
        return false;
    }
    if (b->size + sz <= b->cap) return true;
    next_cap = b->cap;
    do { next_cap *= 2; } while (next_cap < b->size + sz);
    if (b->ptr == b->buffer) {
        uint8_t *next = (uint8_t *)xrtMalloc((size_t)next_cap);
        if (next == NULL) {
            bson_memory_error();
            b->failed = true;
            return false;
        }
        memcpy(next, b->buffer, b->size);
        b->ptr = next;
    } else {
        uint8_t *next = (uint8_t *)xrtRealloc(b->ptr, (size_t)next_cap);
        if (next == NULL) {
            bson_memory_error();
            b->failed = true;
            return false;
        }
        b->ptr = next;
    }
    b->cap = next_cap;
    return true;
}

/* ─── Write primitives ─── */
static void w_byte(struct bson *b, uint8_t v) {
    if (!bson_reserve(b, 1)) return;
    b->ptr[b->size++] = v;
}

static void w_int32(struct bson *b, int32_t v) {
    uint32_t uv = (uint32_t)v;
    if (!bson_reserve(b, 4)) return;
    b->ptr[b->size++] = uv & 0xff;
    b->ptr[b->size++] = (uv >> 8) & 0xff;
    b->ptr[b->size++] = (uv >> 16) & 0xff;
    b->ptr[b->size++] = (uv >> 24) & 0xff;
}

static void w_int64(struct bson *b, int64_t v) {
    uint64_t uv = (uint64_t)v;
    int i;
    if (!bson_reserve(b, 8)) return;
    for (i = 0; i < 64; i += 8)
        b->ptr[b->size++] = (uv >> i) & 0xff;
}

static void w_double(struct bson *b, double d) {
    union { double d; uint64_t i; } v;
    int i;
    v.d = d;
    if (!bson_reserve(b, 8)) return;
    for (i = 0; i < 64; i += 8)
        b->ptr[b->size++] = (v.i >> i) & 0xff;
}

static void w_cstring(struct bson *b, const char *s, int len) {
    if (!bson_reserve(b, len + 1)) return;
    memcpy(b->ptr + b->size, s, len);
    b->ptr[b->size + len] = '\0';
    b->size += len + 1;
}

static void w_length_at(struct bson *b, int32_t v, int off) {
    uint32_t uv = (uint32_t)v;
    if (b->failed || off < 0) return;
    b->ptr[off++] = uv & 0xff;
    b->ptr[off++] = (uv >> 8) & 0xff;
    b->ptr[off++] = (uv >> 16) & 0xff;
    b->ptr[off++] = (uv >> 24) & 0xff;
}

static int w_reserve_length(struct bson *b) {
    int off = b->size;
    if (!bson_reserve(b, 4)) return -1;
    b->size += 4;
    return off;
}

static void w_key(struct bson *b, int type, const char *key, int klen) {
    if (klen < 0 || (klen > 0 && (!key || memchr(key, 0, (size_t)klen) != NULL)) ||
        !xrtUtf8Valid((xstrview){ key, (size_t)klen }, NULL)) {
        b->failed = true;
        bson_error("bson: document key must be UTF-8 without NUL");
        return;
    }
    w_byte(b, (uint8_t)type);
    w_cstring(b, key, klen);
}

/* ─── Read primitives (bounds-safe) ─── */
static int32_t get_length(const uint8_t *data) {
    return (int32_t)(data[0] | data[1]<<8 | data[2]<<16 | ((uint32_t)data[3]<<24));
}

static bool r_need(struct bson_reader *br, int count) {
    if (br->failed || count < 0 || count > br->size) {
        br->failed = true;
        bson_error("bson: truncated or malformed input");
        return false;
    }
    return true;
}

static int r_byte(struct bson_reader *br) {
    if (!r_need(br, 1)) return -1;
    int v = br->ptr[0];
    br->ptr++; br->size--;
    return v;
}

static int32_t r_int32(struct bson_reader *br) {
    if (!r_need(br, 4)) return 0;
    const uint8_t *b = br->ptr;
    uint32_t v = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    br->ptr += 4; br->size -= 4;
    return (int32_t)v;
}

static int64_t r_int64(struct bson_reader *br) {
    if (!r_need(br, 8)) return 0;
    const uint8_t *b = br->ptr;
    uint32_t lo = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    uint32_t hi = b[4] | b[5]<<8 | b[6]<<16 | ((uint32_t)b[7]<<24);
    br->ptr += 8; br->size -= 8;
    return (int64_t)((uint64_t)lo | (uint64_t)hi << 32);
}

static double r_double(struct bson_reader *br) {
    union { uint64_t i; double d; } v;
    if (!r_need(br, 8)) return 0.0;
    const uint8_t *b = br->ptr;
    uint32_t lo = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    uint32_t hi = b[4] | b[5]<<8 | b[6]<<16 | ((uint32_t)b[7]<<24);
    v.i = (uint64_t)lo | (uint64_t)hi << 32;
    br->ptr += 8; br->size -= 8;
    return v.d;
}

static const char *r_cstring(struct bson_reader *br, int *out_len) {
    int i;
    if (br->failed) return NULL;
    for (i = 0; i < br->size; i++) {
        if (br->ptr[i] == '\0') break;
    }
    if (i >= br->size) { *out_len = 0; br->failed = true; bson_error("bson: unterminated key"); return NULL; }
    *out_len = i;
    const char *r = (const char *)br->ptr;
    br->ptr += i + 1;
    br->size -= i + 1;
    return r;
}

/* 每次独立取随机尾部，避免进程全局计数器和惰性初始化的线程竞态。 */
static bool gen_oid(uint8_t *out12) {
    xtime now = xrtNow();
    int64 seconds;
    uint32_t ti;
    if (xrtGetError()) return false;
    seconds = xrtTimeUnix(now);
    if (seconds < 0 || seconds > UINT32_MAX) {
        bson_range_error("bson: ObjectId timestamp is out of range");
        return false;
    }
    ti = (uint32_t)seconds;
    if (!xrtSecureRandom(out12 + 4, 8)) return false;
    out12[0] = (ti >> 24) & 0xff;
    out12[1] = (ti >> 16) & 0xff;
    out12[2] = (ti >> 8) & 0xff;
    out12[3] = ti & 0xff;
    return true;
}

/* ═══════════════════════════════════════════
   BSON Decode: binary → xvalue* (var)
   ═══════════════════════════════════════════ */

static xvalue* bson_decode_doc(struct bson_reader *br);
static xvalue* bson_decode_array(struct bson_reader *br);

static xvalue* bson_decode_value(struct bson_reader *br, int bt) {
    if (br->failed) return NULL;
    switch (bt) {
    case BSON_REAL: {
        double value = r_double(br);
        return br->failed ? NULL : xrtValueFloat(value);
    }
    case BSON_STRING:
    {
        int32_t slen = r_int32(br);
        xvalue* val;
        if (slen < 1 || !r_need(br, slen) || br->ptr[slen - 1] != 0 ||
            !xrtUtf8Valid((xstrview){ (const char*)br->ptr, (size_t)slen - 1 }, NULL)) {
            br->failed = true;
            bson_error("bson: invalid UTF-8 string");
            return NULL;
        }
        val = bson_string((const char*)br->ptr, (size_t)(slen - 1));
        br->ptr += slen; br->size -= slen;
        return val;
    }
    case BSON_DOCUMENT:
        return bson_decode_doc(br);
    case BSON_ARRAY:
        return bson_decode_array(br);
    case BSON_BOOLEAN: {
        int v = r_byte(br);
        if (v != 0 && v != 1) {
            br->failed = true;
            bson_error("bson: boolean must be 0 or 1");
            return NULL;
        }
        return xrtValueRetain(xrtValueBool(v ? true : false));
    }
    case BSON_INT32: {
        int32_t value = r_int32(br);
        return br->failed ? NULL : xrtValueInt(value);
    }
    case BSON_INT64: {
        int64_t value = r_int64(br);
        return br->failed ? NULL : xrtValueInt(value);
    }
    case BSON_DATE: {
        int64_t ms = r_int64(br);
        xtime t;
        /* 截断已是格式错误，不能再转换零值或由后续分配覆盖首因。 */
        if (br->failed) return NULL;
        if (!xrtTimeFromUnixMs(ms, &t)) {
            br->failed = true;
            bson_error("bson: date is outside the XRT time range");
            return NULL;
        }
        return xrtValueTime(t);
    }
    case BSON_NULL:
        return bson_null();
    case BSON_OBJECTID: {
        static const char *hex = "0123456789abcdef";
        char out[25];
        int i;
        if (!r_need(br, 12)) return NULL;
        for (i = 0; i < 12; i++) {
            out[i*2]   = hex[br->ptr[i] >> 4];
            out[i*2+1] = hex[br->ptr[i] & 0xf];
        }
        out[24] = '\0';
        br->ptr += 12; br->size -= 12;
        return bson_string(out, 24);
    }
    case BSON_BINARY: {
        int32_t blen = r_int32(br);
        int subtype = r_byte(br);
        xvalue* val;
        if (subtype != 0) {
            br->failed = true;
            bson_error("bson: only generic binary subtype 0 is supported");
            return NULL;
        }
        if (!r_need(br, blen)) return NULL;
        val = xrtValueBytes((xbytesview){ br->ptr, (size_t)blen });
        br->ptr += blen; br->size -= blen;
        return val;
    }
    case BSON_DBPOINTER: {
        br->failed = true;
        bson_error("bson: DBPointer has no xvalue representation");
        return NULL;
    }
    case BSON_CODEWS: {
        br->failed = true;
        bson_error("bson: JavaScript-with-scope has no xvalue representation");
        return NULL;
    }
    default:
        br->failed = true;
        bson_error("bson: unsupported value type");
        return NULL;
    }
}

/* Validate document header: returns body size (doc_len-5) or -1 on error */
static int bson_validate_header(struct bson_reader *br) {
    int32_t doc_len;
    if (br->size < 5) return -1;
    doc_len = get_length(br->ptr);
    if (doc_len < 5 || doc_len > br->size || doc_len > MAX_BSON_BYTES ||
        br->ptr[doc_len - 1] != 0) return -1;
    return doc_len;
}

static xvalue* bson_decode_container(struct bson_reader *br, bool is_array) {
    int doc_len;
    struct bson_reader sub;
    xvalue* container;
    size_t index = 0;

    doc_len = bson_validate_header(br);
    if (br->failed || doc_len < 0) {
        br->failed = true;
        bson_error("bson: invalid document");
        return NULL;
    }
    if (br->depth >= MAX_DEPTH) {
        br->failed = true;
        bson_range_error("bson: nesting exceeds the depth limit");
        return NULL;
    }
    sub.ptr = br->ptr + 4;
    sub.size = doc_len - 5;
    sub.depth = br->depth + 1;
    sub.failed = false;
    sub.items = br->items;
    br->ptr += doc_len;
    br->size -= doc_len;

    container = is_array ? xvoCreateArray() : xvoCreateTable();
    if (!container) { br->failed = true; return NULL; }
    while (sub.size > 0) {
        int bt = r_byte(&sub);
        int klen = 0;
        const char *key;
        xvalue* val;
        char expected[24];
        if (*sub.items == MAX_BSON_ITEMS) {
            sub.failed = true;
            bson_range_error("bson: document has too many items");
            break;
        }
        ++*sub.items;
        if (bt <= 0) { sub.failed = true; bson_error("bson: unexpected document terminator"); break; }
        key = r_cstring(&sub, &klen);
        if (!key) break;
        if (!xrtUtf8Valid((xstrview){ key, (size_t)klen }, NULL)) {
            sub.failed = true;
            bson_error("bson: key is not valid UTF-8");
            break;
        }
        if (is_array) {
            int length = snprintf(expected, sizeof(expected), "%zu", index);
            if (length != klen || memcmp(key, expected, (size_t)klen) != 0) {
                sub.failed = true;
                bson_error("bson: array keys must be sequential indexes");
                break;
            }
        } else if (xrtValueObjectHas(container, (xstrview){ key, (size_t)klen })) {
            sub.failed = true;
            bson_error("bson: duplicate document key");
            break;
        }
        val = bson_decode_value(&sub, bt);
        if (sub.failed || !val) { xvoUnref(val); sub.failed = true; break; }
        if (is_array ? !xrtValueArrayAppendTake(container, &val) :
            !xrtValueObjectSetTake(container, (xstrview){ key, (size_t)klen }, &val)) {
            xvoUnref(val);
            sub.failed = true;
            break;
        }
        ++index;
    }
    if (sub.failed) {
        br->failed = true;
        xvoUnref(container);
        return NULL;
    }
    return container;
}

static xvalue* bson_decode_doc(struct bson_reader *br) {
    return bson_decode_container(br, false);
}

static xvalue* bson_decode_array(struct bson_reader *br) {
    return bson_decode_container(br, true);
}

/* ═══════════════════════════════════════════
   BSON Encode: xvalue* (var) → binary
   ═══════════════════════════════════════════ */

static void bson_encode_value(struct bson *b, const char *key, int klen,
                              xvalue* val, int depth);

static void bson_encode_value(struct bson *b, const char *key, int klen,
                               xvalue* val, int depth) {
    if (b->failed) return;
    /* depth 从根字段的 0 开始；容器值会再增加一层文档深度。 */
    if (depth >= MAX_DEPTH ||
        (depth == MAX_DEPTH - 1 &&
         (xvoIsTable(val) || xvoIsArray(val) || xvoIsList(val)))) {
        b->failed = true;
        bson_range_error("bson: nesting exceeds the depth limit");
        return;
    }
    if (b->items == MAX_BSON_ITEMS) {
        b->failed = true;
        bson_range_error("bson: document has too many items");
        return;
    }
    ++b->items;

    if (val == NULL || xvoIsNull(val)) {
        w_key(b, BSON_NULL, key, klen);
    } else if (xvoIsBool(val)) {
        w_key(b, BSON_BOOLEAN, key, klen);
        w_byte(b, xvoGetBool(val) ? 1 : 0);
    } else if (xvoIsInt(val)) {
        int64_t v = xvoGetInt(val);
        if (v >= -2147483648LL && v <= 2147483647LL) {
            w_key(b, BSON_INT32, key, klen);
            w_int32(b, (int32_t)v);
        } else {
            w_key(b, BSON_INT64, key, klen);
            w_int64(b, v);
        }
    } else if (xvoIsFloat(val)) {
        w_key(b, BSON_REAL, key, klen);
        w_double(b, xvoGetFloat(val));
    } else if (xvoIsBytes(val)) {
        xbytesview data;
        if (!xrtValueGetBytes(val, &data) || data.Size > MAX_BSON_BYTES) {
            b->failed = true;
            bson_range_error("bson: binary value exceeds the 64 MiB safety limit");
            return;
        }
        /* bytes 始终映射为 BSON 通用二进制 subtype 0。 */
        w_key(b, BSON_BINARY, key, klen);
        w_int32(b, (int32_t)data.Size);
        w_byte(b, 0);
        if (data.Size != 0 && bson_reserve(b, (int)data.Size)) {
            memcpy(b->ptr + b->size, data.Data, data.Size);
            b->size += (int)data.Size;
        }
    } else if (xvoIsText(val)) {
        xstrview text = {0, 0};
        const char *s;
        int slen;
        if (!xrtValueGetString(val, &text)) {
            b->failed = true;
            bson_error("bson: invalid string value");
            return;
        }
        if (text.Size > MAX_BSON_BYTES) {
            b->failed = true;
            bson_range_error("bson: string exceeds the 64 MiB safety limit");
            return;
        }
        s = text.Data;
        if (!s || !xrtUtf8Valid(text, NULL)) {
            b->failed = true;
            bson_error("bson: invalid UTF-8 string");
            return;
        }
        slen = (int)text.Size;
        w_key(b, BSON_STRING, key, klen);
        { int off = w_reserve_length(b);
          w_cstring(b, s ? s : "", slen);
          w_length_at(b, slen + 1, off); }
    } else if (xvoIsTime(val)) {
        xtime t = xvoGetTime(val);
        int64_t ms;
        if (!xrtTimeToUnixMs(t, &ms)) { b->failed = true; return; }
        w_key(b, BSON_DATE, key, klen);
        w_int64(b, ms);
    } else if (xvoIsTable(val)) {
        int off;
        size_t i;
        w_key(b, BSON_DOCUMENT, key, klen);
        off = w_reserve_length(b);
        for (i = 0; i < xrtValueCount(val) && !b->failed; ++i) {
            xstrview itemKey;
            xvalue* item = xrtValueObjectAt(val, i, &itemKey);
            if (item != NULL) {
                if (itemKey.Size > MAX_BSON_BYTES) {
                    b->failed = true;
                    bson_range_error("bson: document key is too large");
                    break;
                }
                bson_encode_value(b, itemKey.Data, (int)itemKey.Size,
                    item, depth + 1);
                /* ObjectAt returns a borrowed value. */
            }
        }
        w_byte(b, 0);
        w_length_at(b, b->size - off, off);
    } else if (xvoIsArray(val)) {
        int off;
        uint32_t i, count;
        char idxbuf[16];
        w_key(b, BSON_ARRAY, key, klen);
        off = w_reserve_length(b);
        count = xvoArrayItemCount(val);
        for (i = 0; i < count && !b->failed; i++) {
            xvalue* item = xvoArrayGetValue(val, i);
            if (!item) { b->failed = true; bson_error("bson: missing array item"); break; }
            int idxlen = sprintf(idxbuf, "%u", i);
            bson_encode_value(b, idxbuf, idxlen, item, depth + 1);
            /* Array/IntMap adapter returned an owned retain. */
            if (item != NULL) xrtValueRelease(item);
        }
        w_byte(b, 0);
        w_length_at(b, b->size - off, off);
    } else if (xvoIsList(val)) {
        int off;
        uint32_t i, count;
        char idxbuf[16];
        w_key(b, BSON_ARRAY, key, klen);
        off = w_reserve_length(b);
        count = xvoListItemCount(val);
        for (i = 0; i < count && !b->failed; i++) {
            xvalue* item = xvoListGetValue(val, (int64)i);
            if (!item) { b->failed = true; bson_error("bson: sparse list cannot become a BSON array"); break; }
            int idxlen = sprintf(idxbuf, "%u", i);
            bson_encode_value(b, idxbuf, idxlen, item, depth + 1);
            /* Array/IntMap adapter returned an owned retain. */
            if (item != NULL) xrtValueRelease(item);
        }
        w_byte(b, 0);
        w_length_at(b, b->size - off, off);
    } else {
        b->failed = true;
        bson_error("bson: dynamic value type is not supported");
    }
}

/* ═══════════════════════════════════════════
   Exported API
   ═══════════════════════════════════════════ */

/* Decode BSON bytes into a document. */
xvalue* xbson_decode_span(xbytesview wire) {
    struct bson_reader br;
    size_t items = 0u;
    xrtClearError();
    if (wire.Data == NULL || wire.Size < 5) {
        bson_error("bson: invalid data"); return xvoCreateNull();
    }
    if (wire.Size > MAX_BSON_BYTES) { bson_range_error("bson: input exceeds the 64 MiB safety limit"); return xvoCreateNull(); }
    br = (struct bson_reader){ wire.Data, (int)wire.Size, 0, false, &items };
    xvalue *result = bson_decode_doc(&br);
    if (br.failed || !result) return xvoCreateNull();
    if (br.size != 0) {
        xvoUnref(result);
        bson_error("bson: trailing data after document");
        return xvoCreateNull();
    }
    return result;
}

xvalue* xbson_decode(xvalue* data) {
    xbytesview wire = {0};
    if (!bson_wire(data, &wire)) {
        xrtClearError();
        bson_error("bson: invalid data");
        return xvoCreateNull();
    }
    return xbson_decode_span(wire);
}

/* Decode concatenated BSON stream → array of tables */
xvalue* xbson_decode_all_span(xbytesview wire) {
    struct bson_reader br;
    size_t items = 0u;
    xvalue* arr;
    xrtClearError();
    if (wire.Data == NULL || wire.Size < 5) {
        bson_error("bson: invalid data"); return xvoCreateNull();
    }
    if (wire.Size > MAX_BSON_BYTES) { bson_range_error("bson: input exceeds the 64 MiB safety limit"); return xvoCreateNull(); }
    br = (struct bson_reader){ wire.Data, (int)wire.Size, 0, false, &items };
    arr = xvoCreateArray();
    if (arr == NULL) return xvoCreateNull();
    while (br.size >= 5) {
        int32_t doc_len = bson_validate_header(&br);
        xvalue* doc;
        if (doc_len < 0) {
            xvoUnref(arr);
            bson_error("bson: invalid document stream");
            return xvoCreateNull();
        }
        doc = bson_decode_doc(&br);
        if (br.failed || doc == NULL || xvoIsNull(doc)) {
            xvoUnref(arr);
            return xvoCreateNull();
        }
        if (!xrtValueArrayAppendTake(arr, &doc)) {
            xvoUnref(doc);
            xvoUnref(arr);
            return xvoCreateNull();
        }
    }
    if (br.size != 0) {
        xvoUnref(arr);
        bson_error("bson: truncated document stream");
        return xvoCreateNull();
    }
    return arr;
}

xvalue* xbson_decode_all(xvalue* data) {
    xbytesview wire = {0};
    if (!bson_wire(data, &wire)) {
        xrtClearError();
        bson_error("bson: invalid data");
        return xvoCreateNull();
    }
    return xbson_decode_all_span(wire);
}

/* Binary-safe concatenation of two BSON vars */
xvalue* xbson_concat_span(xbytesview first, xbytesview second) {
    size_t total;
    unsigned char *buf;
    xrtClearError();
    if (first.Data == NULL || second.Data == NULL) {
        bson_error("bson: concat requires two BSON documents");
        return xvoCreateNull();
    }
    if (first.Size < 5 || second.Size < 5) {
        bson_error("bson: concat requires two valid BSON documents");
        return xvoCreateNull();
    }
    if (first.Size > MAX_BSON_BYTES || second.Size > MAX_BSON_BYTES ||
        first.Size > MAX_BSON_BYTES - second.Size) {
        bson_range_error("bson: concatenated stream exceeds the 64 MiB safety limit");
        return xvoCreateNull();
    }
    if (first.Data[first.Size - 1] != 0 ||
        second.Data[second.Size - 1] != 0 ||
        get_length(first.Data) != (int32_t)first.Size ||
        get_length(second.Data) != (int32_t)second.Size) {
        bson_error("bson: concat requires two valid BSON documents");
        return xvoCreateNull();
    }
    total = first.Size + second.Size;
    buf = (unsigned char *)xrtMalloc(total);
    if (!buf) { bson_memory_error(); return xvoCreateNull(); }
    memcpy(buf, first.Data, first.Size);
    memcpy(buf + first.Size, second.Data, second.Size);
    return bson_bytes_take(buf, total);
}

xvalue* xbson_concat(xvalue* a, xvalue* b) {
    xbytesview first = {0}, second = {0};
    if (!bson_wire(a, &first) || !bson_wire(b, &second)) {
        xrtClearError();
        bson_error("bson: concat requires two BSON documents");
        return xvoCreateNull();
    }
    return xbson_concat_span(first, second);
}

/* Encode var (table) into BSON bytes. */
xvalue* xbson_encode(xvalue* val) {
    struct bson b;
    int off;
    int32_t total;
    char *result;

    xrtClearError();
    if (val == NULL || !xvoIsTable(val)) {
        bson_error("bson: encode requires a table (var dict)");
        return xvoCreateNull();
    }
    bson_init(&b);
    off = w_reserve_length(&b);
    {
        size_t i;
        for (i = 0; i < xrtValueCount(val) && !b.failed; ++i) {
            xstrview itemKey;
            xvalue* item = xrtValueObjectAt(val, i, &itemKey);
            if (item != NULL) {
                if (itemKey.Size > MAX_BSON_BYTES) {
                    b.failed = true;
                    bson_range_error("bson: document key is too large");
                    break;
                }
                bson_encode_value(&b, itemKey.Data, (int)itemKey.Size,
                    item, 0);
                /* ObjectAt returns a borrowed value. */
            }
        }
    }
    w_byte(&b, 0);
    if (b.failed) {
        bson_free(&b);
        return xvoCreateNull();
    }
    total = b.size;
    w_length_at(&b, total, off);

    if (b.ptr == b.buffer) {
        result = (char *)xrtMalloc(total);
        if (!result) { bson_free(&b); bson_memory_error(); return xvoCreateNull(); }
        memcpy(result, b.ptr, total);
    } else {
        /* 堆缓冲直接交给结果值；析构写入器时不能再释放。 */
        result = (char *)b.ptr;
        b.ptr = b.buffer;
    }
    bson_free(&b);
    return bson_bytes_take(result, (size_t)total);
}

/* Load BSON file → var (table) */
xvalue* xbson_load(const char *path) {
    unsigned char *buf;
    size_t size;
    size_t items = 0u;
    struct bson_reader br;
    xvalue* result;

    xrtClearError();
    if (!path) { bson_error("bson: null path"); return xvoCreateNull(); }
    buf = xrtFileReadAllLimit(path, MAX_BSON_BYTES, &size);
    if (!buf) return xvoCreateNull();
    if (size < 5 || size > MAX_BSON_BYTES) {
        xrtFree(buf);
        if (size > MAX_BSON_BYTES) {
            bson_range_error("bson: file exceeds the 64 MiB safety limit");
        } else {
            bson_error("bson: file size is invalid");
        }
        return xvoCreateNull();
    }
    br = (struct bson_reader){ (const uint8_t *)buf, (int)size, 0, false, &items };
    result = bson_decode_doc(&br);
    if (result && br.size != 0) {
        xvoUnref(result);
        result = NULL;
        bson_error("bson: trailing data after document");
    }
    xrtFree(buf);
    return result ? result : xvoCreateNull();
}

/* Save var (table) → BSON file */
int64_t xbson_save(const char *path, xvalue* val) {
    xvalue* encoded;
    xbytesview wire;

    if (!path) { bson_error("bson: null path"); return 0; }
    encoded = xbson_encode(val);
    if (encoded == NULL || xvoIsNull(encoded)) return 0;
    if (!bson_wire(encoded, &wire) || wire.Size < 5) {
        xvoUnref(encoded); bson_error("bson: encoded document is invalid"); return 0;
    }
    if (!xrtFileWriteAtomic(path, wire)) {
        xvoUnref(encoded);
        return 0;
    }
    xvoUnref(encoded);
    return (int64_t)wire.Size;
}

/* BSON binary var → hex string (debug) */
char *xbson_to_hex_span(xbytesview wire) {
    static const char *hex = "0123456789abcdef";
    char *out;
    size_t i;
    xrtClearError();
    if (wire.Data == NULL || wire.Size == 0) {
        bson_error("bson: invalid data"); return NULL;
    }
    if (wire.Size > MAX_BSON_BYTES) {
        bson_range_error("bson: input exceeds the 64 MiB safety limit");
        return NULL;
    }
    if (wire.Size > MAX_BSON_BYTES / 2u) {
        bson_range_error("bson: hex output exceeds the 64 MiB safety limit");
        return NULL;
    }
    out = (char *)xrtMalloc(wire.Size * 2u + 1u);
    if (!out) return NULL;
    for (i = 0; i < wire.Size; i++) {
        out[i*2]   = hex[wire.Data[i] >> 4];
        out[i*2+1] = hex[wire.Data[i] & 0xf];
    }
    out[wire.Size*2] = '\0';
    return out;
}

char *xbson_to_hex(xvalue* data) {
    xbytesview wire = {0};
    if (!bson_wire(data, &wire)) {
        xrtClearError();
        bson_error("bson: invalid data");
        return NULL;
    }
    return xbson_to_hex_span(wire);
}

/* Generate new ObjectId (24-char hex string) */
char *xbson_gen_oid(void) {
    uint8_t oid[12];
    static const char *hex = "0123456789abcdef";
    char *out;
    int i;
    xrtClearError();
    out = (char *)xrtMalloc(25);
    if (!out) return NULL;
    if (!gen_oid(oid)) { xrtFree(out); return NULL; }
    for (i = 0; i < 12; i++) {
        out[i*2]   = hex[oid[i] >> 4];
        out[i*2+1] = hex[oid[i] & 0xf];
    }
    out[24] = '\0';
    return out;
}

void* xbson_to_hex_abi(xvalue* data) { return xbson_to_hex(data); }
void* xbson_gen_oid_abi(void) { return xbson_gen_oid(); }

/* Get BSON document length from binary var header */
int64_t xbson_doc_length_span(xbytesview wire) {
    xrtClearError();
    if (wire.Data == NULL || wire.Size < 5 ||
        wire.Size > MAX_BSON_BYTES || wire.Data[wire.Size - 1] != 0 ||
        get_length(wire.Data) != (int32_t)wire.Size) return 0;
    return (int64_t)wire.Size;
}

int64_t xbson_doc_length(xvalue* data) {
    xbytesview wire = {0};
    if (!bson_wire(data, &wire)) {
        xrtClearError();
        return 0;
    }
    return xbson_doc_length_span(wire);
}

/* Extract timestamp from ObjectId hex → xtime */
int64_t xbson_oid_time(const char *hex24) {
    uint32_t ts = 0;
    size_t length = 0u;
    int i;
    xrtClearError();
    if (hex24 != NULL)
        while (length <= 24u && hex24[length] != '\0') ++length;
    if (length != 24u) {
        bson_error("bson: ObjectId must contain exactly 24 hexadecimal characters");
        return -1;
    }
    for (i = 0; i < 24; i++) {
        char c = hex24[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else {
            bson_error("bson: ObjectId must contain exactly 24 hexadecimal characters");
            return -1;
        }
        if (i < 8) ts = (ts << 4) | (uint32_t)d;
    }
    return (int64_t)ts;
}

void* xbson_to_hex_span_abi(xbytesview data) { return xbson_to_hex_span(data); }
