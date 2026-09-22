/*
 * bson_native.c - BSON <-> xlang var bidirectional codec
 * Based on cloudwu/lua-bson (MIT License) - buffer management & primitives
 * Architecture: BSON binary <-> xvalue* (table/list/primitives) direct conversion
 */
#include "xl_runtime.h"

#include <string.h>
#include <stdio.h>
#include <time.h>



static void bson_error(const char *message) {
    xrtSetErrorInfo(3, "xlang.extlib.bson", 0, message);
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

static const char *xvoGetText(const xvalue *value) {
    xstrview result = { 0, 0 };
    return xrtValueGetString(value, &result) ? result.Data : NULL;
}

static uint32 xvoGetSize(const xvalue *value) {
    xstrview result = { 0, 0 };
    return xrtValueGetString(value, &result) && result.Size <= UINT32_MAX
        ? (uint32)result.Size : 0u;
}

static xvalue *bson_string(const char *data, size_t size) {
    return xrtValueString((xstrview){ data != NULL ? data : "", size });
}

static xvalue *xvoCreateNull(void) {
    return bson_null();
}

static xvalue *xvoCreateText(ptr data, uint32 size, bool take) {
    if (take) {
        str owned = (str)data;
        xvalue *value = xrtValueStringTake(&owned, (size_t)size);
        if (value == NULL && owned != NULL) {
            xrtFree(owned);
        }
        return value;
    }
    return bson_string((const char *)data, (size_t)size);
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

static bool xvoArrayAppendValue(xvalue *array, xvalue *value, bool take) {
    return take ? xrtValueArrayAppendTake(array, &value)
        : xrtValueArrayAppend(array, value);
}

static uint32 xvoArrayItemCount(const xvalue *array) {
    size_t count = xrtValueCount(array);
    return count <= UINT32_MAX ? (uint32)count : UINT32_MAX;
}

static xvalue *xvoArrayGetValue(const xvalue *array, uint32 index) {
    xvalue *value = xrtValueArrayGet(array, (size_t)index);
    return value != NULL ? xrtValueRetain(value) : NULL;
}

static bool xvoTableSetValue(xvalue *table, const void *key, uint32 keySize,
    xvalue *value, bool take) {
    xstrview name = { (const char *)key, (size_t)keySize };
    return take ? xrtValueObjectSetTake(table, name, &value)
        : xrtValueObjectSet(table, name, value);
}

static uint32 xvoListItemCount(const xvalue *list) {
    size_t count = xrtValueCount(list);
    return count <= UINT32_MAX ? (uint32)count : UINT32_MAX;
}

static xvalue *xvoListGetValue(const xvalue *list, int64 index) {
    xvalue *value = xrtValueIntMapGet(list, index);
    return value != NULL ? xrtValueRetain(value) : NULL;
}

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <process.h>
#define XBSON_PID _getpid()
#else
#include <unistd.h>
#define XBSON_PID getpid()
#endif

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
#define MAX_DEPTH   128

/* ─── Dynamic buffer (from lua-bson) ─── */
struct bson {
    int size;
    int cap;
    bool failed;
    uint8_t *ptr;
    uint8_t buffer[DEFAULT_CAP];
};

struct bson_reader {
    const uint8_t *ptr;
    int size;
};

static void bson_init(struct bson *b) {
    b->size = 0;
    b->cap = DEFAULT_CAP;
    b->failed = false;
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
    if (b->size + sz <= b->cap) return true;
    next_cap = b->cap;
    do { next_cap *= 2; } while (next_cap <= b->size + sz);
    if (b->ptr == b->buffer) {
        uint8_t *next = (uint8_t *)xrtMalloc((size_t)next_cap);
        if (next == NULL) {
            bson_error("bson: out of memory");
            b->failed = true;
            return false;
        }
        memcpy(next, b->buffer, b->size);
        b->ptr = next;
    } else {
        uint8_t *next = (uint8_t *)xrtRealloc(b->ptr, (size_t)next_cap);
        if (next == NULL) {
            bson_error("bson: out of memory");
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
    w_byte(b, (uint8_t)type);
    w_cstring(b, key, klen);
}

/* ─── Read primitives (bounds-safe) ─── */
static int32_t get_length(const uint8_t *data) {
    return (int32_t)(data[0] | data[1]<<8 | data[2]<<16 | ((uint32_t)data[3]<<24));
}

static int r_byte(struct bson_reader *br) {
    if (br->size < 1) { br->size = 0; return -1; }
    int v = br->ptr[0];
    br->ptr++; br->size--;
    return v;
}

static int32_t r_int32(struct bson_reader *br) {
    if (br->size < 4) { br->size = 0; return 0; }
    const uint8_t *b = br->ptr;
    uint32_t v = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    br->ptr += 4; br->size -= 4;
    return (int32_t)v;
}

static int64_t r_int64(struct bson_reader *br) {
    if (br->size < 8) { br->size = 0; return 0; }
    const uint8_t *b = br->ptr;
    uint32_t lo = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    uint32_t hi = b[4] | b[5]<<8 | b[6]<<16 | ((uint32_t)b[7]<<24);
    br->ptr += 8; br->size -= 8;
    return (int64_t)((uint64_t)lo | (uint64_t)hi << 32);
}

static double r_double(struct bson_reader *br) {
    union { uint64_t i; double d; } v;
    if (br->size < 8) { br->size = 0; return 0.0; }
    const uint8_t *b = br->ptr;
    uint32_t lo = b[0] | b[1]<<8 | b[2]<<16 | ((uint32_t)b[3]<<24);
    uint32_t hi = b[4] | b[5]<<8 | b[6]<<16 | ((uint32_t)b[7]<<24);
    v.i = (uint64_t)lo | (uint64_t)hi << 32;
    br->ptr += 8; br->size -= 8;
    return v.d;
}

static const char *r_cstring(struct bson_reader *br, int *out_len) {
    int i;
    for (i = 0; i < br->size; i++) {
        if (br->ptr[i] == '\0') break;
    }
    if (i >= br->size) { *out_len = 0; br->size = 0; return ""; }
    *out_len = i;
    const char *r = (const char *)br->ptr;
    br->ptr += i + 1;
    br->size -= i + 1;
    return r;
}

/* Bounds-safe skip: clamp to available size */
static void r_skip(struct bson_reader *br, int n) {
    if (n < 0 || n > br->size) { br->size = 0; return; }
    br->ptr += n;
    br->size -= n;
}

/* ─── OID generation ─── */
static uint8_t oid_header[5];
static uint32_t oid_counter;
static int oid_inited = 0;

static void init_oid(void) {
    uint32_t h = 0;
    int pid;
    if (oid_inited) return;
#if defined(_WIN32) || defined(_WIN64)
    {
        char hostname[256];
        DWORD hlen = sizeof(hostname);
        if (GetComputerNameA(hostname, &hlen)) {
            int i;
            for (i = 0; hostname[i]; i++)
                h = h ^ ((h << 5) + (h >> 2) + hostname[i]);
            h ^= i;
        }
    }
#else
    {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            int i;
            for (i = 0; hostname[i]; i++)
                h = h ^ ((h << 5) + (h >> 2) + hostname[i]);
            h ^= i;
        }
    }
#endif
    pid = (int)XBSON_PID;
    oid_header[0] = h & 0xff;
    oid_header[1] = (h >> 8) & 0xff;
    oid_header[2] = (h >> 16) & 0xff;
    oid_header[3] = pid & 0xff;
    oid_header[4] = (pid >> 8) & 0xff;
    oid_counter = h ^ (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&h;
    oid_inited = 1;
}

static void gen_oid(uint8_t *out12) {
    time_t ti = time(NULL);
    init_oid();
    out12[0] = (ti >> 24) & 0xff;
    out12[1] = (ti >> 16) & 0xff;
    out12[2] = (ti >> 8) & 0xff;
    out12[3] = ti & 0xff;
    memcpy(out12 + 4, oid_header, 5);
    out12[9]  = (oid_counter >> 16) & 0xff;
    out12[10] = (oid_counter >> 8) & 0xff;
    out12[11] = oid_counter & 0xff;
    oid_counter++;
}

/* ═══════════════════════════════════════════
   BSON Decode: binary → xvalue* (var)
   ═══════════════════════════════════════════ */

static xvalue* bson_decode_doc(struct bson_reader *br);
static xvalue* bson_decode_array(struct bson_reader *br);

static xvalue* bson_decode_value(struct bson_reader *br, int bt) {
    switch (bt) {
    case BSON_REAL:
        return xrtValueFloat(r_double(br));
    case BSON_STRING:
    case BSON_JSCODE:
    case BSON_SYMBOL: {
        int32_t slen = r_int32(br);
        xvalue* val;
        if (slen <= 0) return bson_string("", 0);
        if (slen > br->size) { br->size = 0; return bson_string("", 0); }
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
        return xrtValueRetain(xrtValueBool(v ? true : false));
    }
    case BSON_INT32:
        return xrtValueInt((int64)r_int32(br));
    case BSON_INT64:
    case BSON_TIMESTAMP:
        return xrtValueInt(r_int64(br));
    case BSON_DATE: {
        int64_t ms = r_int64(br);
        xtime t;
        if (!xrtTimeFromUnixMs(ms, &t)) {
            bson_error("bson: date is outside the XRT time range");
            return bson_null();
        }
        return xrtValueTime(t);
    }
    case BSON_NULL:
    case BSON_UNDEFINED:
    case BSON_MINKEY:
    case BSON_MAXKEY:
        return bson_null();
    case BSON_OBJECTID: {
        static const char *hex = "0123456789abcdef";
        char out[25];
        int i;
        if (br->size < 12) { br->size = 0; return bson_null(); }
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
        (void)subtype;
        if (blen < 0 || blen > br->size) { br->size = 0; return bson_string("", 0); }
        val = bson_string((const char*)br->ptr, (size_t)blen);
        br->ptr += blen; br->size -= blen;
        return val;
    }
    case BSON_REGEX: {
        int rl1 = 0, rl2 = 0;
        const char *p1 = r_cstring(br, &rl1);
        const char *p2 = r_cstring(br, &rl2);
        /* encode as "/pattern/options" string */
        int total = 1 + rl1 + 1 + rl2 + 1;
        char *buf = (char *)xrtMalloc((size_t)total + 1u);
        xvalue* val;
        buf[0] = '/';
        memcpy(buf + 1, p1, rl1);
        buf[1 + rl1] = '/';
        memcpy(buf + 2 + rl1, p2, rl2);
        buf[total - 1] = '\0';
        val = bson_string(buf, (size_t)(total - 1));
        xrtFree(buf);
        return val;
    }
    case BSON_DBPOINTER: {
        int32_t slen = r_int32(br);
        r_skip(br, slen + 12);
        return xvoCreateNull();
    }
    case BSON_CODEWS: {
        int32_t sz = r_int32(br);
        r_skip(br, sz - 4);
        return xvoCreateNull();
    }
    default:
        return xvoCreateNull();
    }
}

/* Validate document header: returns body size (doc_len-5) or -1 on error */
static int bson_validate_header(struct bson_reader *br) {
    int32_t doc_len;
    if (br->size < 5) return -1;
    doc_len = get_length(br->ptr);
    if (doc_len < 5 || doc_len > br->size) return -1;
    return doc_len;
}

static xvalue* bson_decode_doc(struct bson_reader *br) {
    int32_t doc_len;
    struct bson_reader sub;
    xvalue* tbl;

    if (br->size < 5) {
        bson_error("bson: truncated document");
        return xvoCreateNull();
    }
    doc_len = r_int32(br);
    if (doc_len < 5 || doc_len - 4 > br->size || br->ptr[doc_len - 5] != 0) {
        bson_error("bson: invalid document");
        return xvoCreateNull();
    }

    sub.ptr = br->ptr;
    sub.size = doc_len - 5; /* exclude length(4) and terminator(1) */
    br->ptr += doc_len - 4; /* skip body + terminator */
    br->size -= doc_len - 4;

    tbl = xvoCreateTable();
    while (sub.size > 0) {
        int bt = r_byte(&sub);
        int klen = 0;
        const char *key;
        xvalue* val;
        if (bt == 0 || bt < 0) break;
        key = r_cstring(&sub, &klen);
        val = bson_decode_value(&sub, bt);
        xvoTableSetValue(tbl, (const void*)key, (uint32)klen, val, true);
    }
    return tbl;
}

/* Decode BSON array → xlang array (0-based indexed) */
static xvalue* bson_decode_array(struct bson_reader *br) {
    int32_t doc_len;
    struct bson_reader sub;
    xvalue* arr;

    if (br->size < 5) return xvoCreateArray();
    doc_len = r_int32(br);
    if (doc_len < 5 || doc_len - 4 > br->size) return xvoCreateArray();

    sub.ptr = br->ptr;
    sub.size = doc_len - 5;
    br->ptr += doc_len - 4;
    br->size -= doc_len - 4;

    arr = xvoCreateArray();
    while (sub.size > 0) {
        int bt = r_byte(&sub);
        int klen = 0;
        xvalue* val;
        if (bt == 0 || bt < 0) break;
        r_cstring(&sub, &klen); /* skip index key "0","1",... */
        val = bson_decode_value(&sub, bt);
        xvoArrayAppendValue(arr, val, true);
    }
    return arr;
}

/* ═══════════════════════════════════════════
   BSON Encode: xvalue* (var) → binary
   ═══════════════════════════════════════════ */

static void bson_encode_value(struct bson *b, const char *key, int klen,
                              xvalue* val, int depth);

/* Table walk context for encoding */
struct encode_ctx {
    struct bson *b;
    int depth;
};

static void bson_encode_value(struct bson *b, const char *key, int klen,
                               xvalue* val, int depth) {
    if (depth > MAX_DEPTH) return;

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
    } else if (xvoIsText(val)) {
        const char *s = (const char *)xvoGetText(val);
        uint32_t vsz = xvoGetSize(val);
        int slen = vsz > 0 ? (int)vsz : (s ? (int)strlen(s) : 0);
        w_key(b, BSON_STRING, key, klen);
        { int off = w_reserve_length(b);
          w_cstring(b, s ? s : "", slen);
          w_length_at(b, slen + 1, off); }
    } else if (xvoIsTime(val)) {
        xtime t = xvoGetTime(val);
        int64_t ms = xrtTimeUnixMs(t);
        w_key(b, BSON_DATE, key, klen);
        w_int64(b, ms);
    } else if (xvoIsTable(val)) {
        int off;
        size_t i;
        w_key(b, BSON_DOCUMENT, key, klen);
        off = w_reserve_length(b);
        for (i = 0; i < xrtValueCount(val); ++i) {
            xstrview itemKey;
            xvalue* item = xrtValueObjectAt(val, i, &itemKey);
            if (item != NULL) {
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
        for (i = 0; i < count; i++) {
            xvalue* item = xvoArrayGetValue(val, i);
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
        for (i = 0; i < count; i++) {
            xvalue* item = xvoListGetValue(val, (int64)i);
            int idxlen = sprintf(idxbuf, "%u", i);
            bson_encode_value(b, idxbuf, idxlen, item, depth + 1);
            /* Array/IntMap adapter returned an owned retain. */
            if (item != NULL) xrtValueRelease(item);
        }
        w_byte(b, 0);
        w_length_at(b, b->size - off, off);
    } else {
        /* unsupported type → null */
        w_key(b, BSON_NULL, key, klen);
    }
}

/* ═══════════════════════════════════════════
   Exported API
   ═══════════════════════════════════════════ */

/* Decode BSON binary → var (table); accepts xvalue* text with embedded nulls */
xvalue* xbson_decode(xvalue* data) {
    struct bson_reader br;
    const char *ptr;
    uint32_t len;
    if (data == NULL || xvoIsNull(data)) { bson_error("bson: invalid data"); return xvoCreateNull(); }
    ptr = (const char *)xvoGetText(data);
    len = xvoGetSize(data);
    if (!ptr || len < 5) { bson_error("bson: invalid data"); return xvoCreateNull(); }
    br.ptr = (const uint8_t *)ptr;
    br.size = (int)len;
    return bson_decode_doc(&br);
}

/* Decode concatenated BSON stream → array of tables */
xvalue* xbson_decode_all(xvalue* data) {
    struct bson_reader br;
    const char *ptr;
    uint32_t len;
    xvalue* arr;
    if (data == NULL || xvoIsNull(data)) { bson_error("bson: invalid data"); return xvoCreateNull(); }
    ptr = (const char *)xvoGetText(data);
    len = xvoGetSize(data);
    if (!ptr || len < 5) { bson_error("bson: invalid data"); return xvoCreateNull(); }
    br.ptr = (const uint8_t *)ptr;
    br.size = (int)len;
    arr = xvoCreateArray();
    while (br.size >= 5) {
        int32_t doc_len = bson_validate_header(&br);
        xvalue* doc;
        if (doc_len < 0) {
            xvoUnref(arr);
            bson_error("bson: invalid document stream");
            return xvoCreateNull();
        }
        doc = bson_decode_doc(&br);
        if (doc == NULL || xvoIsNull(doc)) {
            xvoUnref(arr);
            return xvoCreateNull();
        }
        xvoArrayAppendValue(arr, doc, true);
    }
    if (br.size != 0) {
        xvoUnref(arr);
        bson_error("bson: truncated document stream");
        return xvoCreateNull();
    }
    return arr;
}

/* Binary-safe concatenation of two BSON vars */
xvalue* xbson_concat(xvalue* a, xvalue* b) {
    const char *pa, *pb;
    uint32_t la, lb, total;
    char *buf;
    if (a == NULL || xvoIsNull(a)) return b ? b : xvoCreateNull();
    if (b == NULL || xvoIsNull(b)) return a;
    pa = (const char *)xvoGetText(a); la = xvoGetSize(a);
    pb = (const char *)xvoGetText(b); lb = xvoGetSize(b);
    if (!pa || la == 0) return b;
    if (!pb || lb == 0) return a;
    total = la + lb;
    buf = (char *)xrtMalloc(total);
    if (!buf) { bson_error("bson: out of memory"); return xvoCreateNull(); }
    memcpy(buf, pa, la);
    memcpy(buf + la, pb, lb);
    return xvoCreateText((ptr)buf, total, true);
}

/* Encode var (table) → BSON binary (returned as var text with explicit length) */
xvalue* xbson_encode(xvalue* val) {
    struct bson b;
    int off;
    struct encode_ctx ctx;
    int32_t total;
    char *result;

    if (val == NULL || !xvoIsTable(val)) {
        bson_error("bson: encode requires a table (var dict)");
        return xvoCreateNull();
    }
    bson_init(&b);
    off = w_reserve_length(&b);
    ctx.b = &b;
    ctx.depth = 0;
    {
        size_t i;
        for (i = 0; i < xrtValueCount(val); ++i) {
            xstrview itemKey;
            xvalue* item = xrtValueObjectAt(val, i, &itemKey);
            if (item != NULL) {
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

    result = (char *)xrtMalloc(total);
    if (!result) { bson_free(&b); bson_error("bson: out of memory"); return xvoCreateNull(); }
    memcpy(result, b.ptr, total);
    bson_free(&b);
    return xvoCreateText((ptr)result, (uint32)total, true);
}

/* Load BSON file → var (table) */
xvalue* xbson_load(const char *path) {
    unsigned char *buf;
    size_t size;
    struct bson_reader br;
    xvalue* result;

    if (!path) { bson_error("bson: null path"); return xvoCreateNull(); }
    buf = xrtFileReadAll(path, &size);
    if (!buf) return xvoCreateNull();
    if (size < 5 || size > (size_t)INT32_MAX) {
        xrtFree(buf);
        bson_error("bson: file size is invalid");
        return xvoCreateNull();
    }
    br.ptr = (const uint8_t *)buf;
    br.size = (int)size;
    result = bson_decode_doc(&br);
    xrtFree(buf);
    return result;
}

/* Save var (table) → BSON file */
long long xbson_save(const char *path, xvalue* val) {
    xvalue* encoded;
    const char *data;
    uint32_t len;

    encoded = xbson_encode(val);
    if (encoded == NULL || xvoIsNull(encoded)) return 0;
    data = (const char *)xvoGetText(encoded);
    len = xvoGetSize(encoded);
    if (!data || len < 5) { xvoUnref(encoded); bson_error("bson: encoded document is invalid"); return 0; }
    if (!xrtFileWriteAll(path, (xbytesview){
            (const unsigned char *)data, (size_t)len })) {
        xvoUnref(encoded);
        return 0;
    }
    xvoUnref(encoded);
    return (long long)len;
}

/* BSON binary var → hex string (debug) */
const char *xbson_to_hex(xvalue* data) {
    static const char *hex = "0123456789abcdef";
    const char *ptr;
    uint32_t n;
    char *out;
    uint32_t i;
    if (data == NULL || xvoIsNull(data)) return "";
    ptr = (const char *)xvoGetText(data);
    n = xvoGetSize(data);
    if (!ptr || n == 0) return "";
    out = (char *)xrtTemp(n * 2 + 1);
    if (!out) return "";
    for (i = 0; i < n; i++) {
        out[i*2]   = hex[((uint8_t)ptr[i]) >> 4];
        out[i*2+1] = hex[((uint8_t)ptr[i]) & 0xf];
    }
    out[n*2] = '\0';
    return out;
}

/* Generate new ObjectId (24-char hex string) */
const char *xbson_gen_oid(void) {
    uint8_t oid[12];
    static const char *hex = "0123456789abcdef";
    char *out = (char *)xrtTemp(25);
    int i;
    if (!out) return "";
    gen_oid(oid);
    for (i = 0; i < 12; i++) {
        out[i*2]   = hex[oid[i] >> 4];
        out[i*2+1] = hex[oid[i] & 0xf];
    }
    out[24] = '\0';
    return out;
}

/* Get BSON document length from binary var header */
long long xbson_doc_length(xvalue* data) {
    const char *ptr;
    if (data == NULL || xvoIsNull(data)) return 0;
    ptr = (const char *)xvoGetText(data);
    if (!ptr || xvoGetSize(data) < 4) return 0;
    return (long long)get_length((const uint8_t *)ptr);
}

/* Extract timestamp from ObjectId hex → xtime */
long long xbson_oid_time(const char *hex24) {
    uint32_t ts = 0;
    int i;
    if (!hex24 || strlen(hex24) != 24) {
        bson_error("bson: ObjectId must contain exactly 24 hexadecimal characters");
        return 0;
    }
    for (i = 0; i < 24; i++) {
        char c = hex24[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else {
            bson_error("bson: ObjectId must contain exactly 24 hexadecimal characters");
            return 0;
        }
        if (i < 8) ts = (ts << 4) | (uint32_t)d;
    }
    return (long long)ts;
}
