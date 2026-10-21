/*
 * 普通 Bloom 使用全局位数组和两个 XRT 哈希种子；512 位仅是分配粒度。
 * 分块内探针的负载波动会显著抬高低目标误判率，因此不采用旧布局。
 * 热路径不分配内存；对象本身不提供并发同步。
 * 计数版保留独立的 4-bit 计数器，单槽饱和后删除不能完全逆转。
 */

#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#include "xrt.h"
#include "include/bloom.h"

/* ------------------------------------------------------------------ */
/* XRT runtime imports                                                 */
/* ------------------------------------------------------------------ */
static void bloom_error(xerrkind kind, const char *msg) {
    xrtSetErrorInfo(kind, "xlang.extlib.bloom", 0, msg);
}

/* ------------------------------------------------------------------ */
/* Shared math / serialization helpers                                 */
/* ------------------------------------------------------------------ */

/* Portable 64-bit population count (SWAR). Avoids compiler builtins so
 * behavior is identical under TCC and the host compiler. */
static unsigned long long bloom_popcount64(unsigned long long x) {
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (x * 0x0101010101010101ULL) >> 56;
}

static void bloom_put_u32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static void bloom_put_u64(unsigned char *p, unsigned long long v) {
    int i;
    for (i = 0; i < 8; i++) { p[i] = (unsigned char)(v & 0xFFULL); v >>= 8; }
}

static unsigned int bloom_get_u32(const unsigned char *p) {
    return (unsigned int)p[0]
         | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16)
         | ((unsigned int)p[3] << 24);
}

static unsigned long long bloom_get_u64(const unsigned char *p) {
    unsigned long long v = 0;
    int i;
    for (i = 7; i >= 0; i--) { v = (v << 8) | (unsigned long long)p[i]; }
    return v;
}

static const char BLOOM_HEX_DIGITS[] = "0123456789abcdef";

static void bloom_hex_byte(char *out, unsigned char v) {
    out[0] = BLOOM_HEX_DIGITS[(v >> 4) & 0x0F];
    out[1] = BLOOM_HEX_DIGITS[v & 0x0F];
}

static int bloom_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a hex string into a freshly malloc'd byte buffer.
 * Returns NULL on invalid input; caller frees. *out_len receives byte count. */
static unsigned char *bloom_hex_decode(const char *hex, size_t *out_len) {
    size_t hex_len, n, i;
    unsigned char *buf;
    if (!hex) { bloom_error(XERR_VALUE, "bloom: invalid serialized data"); return NULL; }
    hex_len = strlen(hex);
    if ((hex_len % 2) != 0) { bloom_error(XERR_VALUE, "bloom: invalid serialized data"); return NULL; }
    n = hex_len / 2;
    buf = (unsigned char *)xrtMalloc(n ? n : 1);
    if (!buf) { bloom_error(XERR_MEMORY, "bloom: decoding allocation failed"); return NULL; }
    for (i = 0; i < n; i++) {
        int hi = bloom_hex_val(hex[i * 2]);
        int lo = bloom_hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            xrtFree(buf); bloom_error(XERR_VALUE, "bloom: invalid serialized data"); return NULL;
        }
        buf[i] = (unsigned char)((hi << 4) | lo);
    }
    *out_len = n;
    return buf;
}

/* Serialization format (little-endian, hex-encoded on the wire):
 *   u32 magic | u32 version | u32 nhashes | u32 reserved
 *   u64 size-field (num_blocks or nslots) | u64 count | u64 capacity
 *   followed by the raw bit-array / counter bytes.
 * Header is 40 bytes. */
#define BLOOM_HDR_BYTES   40u
#define BLOOM_MAGIC       0x4D4C4258u   /* "XBLM" */
#define BLOOM_CB_MAGIC    0x4D434258u   /* "XBCM" */
#define BLOOM_VERSION     2u
#define BLOOM_CB_VERSION  1u

/* ------------------------------------------------------------------ */
/* Bloom storage chunk constants                                        */
/* ------------------------------------------------------------------ */
#define BLOOM_BLOCK_BITS   512u                       /* allocation granularity */
#define BLOOM_BLOCK_WORDS  8u                         /* 512 / 64 */
#define BLOOM_BLOCK_BYTES  64u                        /* 512 / 8 */

/* ------------------------------------------------------------------ */
/* Bloom Filter (global bit array)                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned long long *blocks;
    unsigned long long num_blocks;
    unsigned long long nbits;      /* num_blocks * 512, for reporting */
    unsigned int nhashes;          /* probes per element (k) */
    unsigned long long count;
    unsigned long long capacity;
} XBloom;

static unsigned long long bloom_optimal_bits(long long n, double fp) {
    double m;
    unsigned long long bits;
    if (n <= 0 || !(fp > 0.0 && fp < 1.0)) {
        bloom_error(XERR_VALUE, "bloom: capacity must be positive and false-positive rate must be between 0 and 1");
        return 0;
    }
    m = -(double)n * log(fp) / (log(2.0) * log(2.0));
    /* 在浮点转整数、对齐及序列化长度计算前拒绝不可表示的尺寸。 */
    if (!(m < (double)LLONG_MAX)) {
        bloom_error(XERR_RANGE, "bloom: capacity exceeds the representable filter size");
        return 0;
    }
    bits = (unsigned long long)(m + 0.5);
    if (bits < BLOOM_BLOCK_BITS) bits = BLOOM_BLOCK_BITS;
    return bits;
}

static int bloom_body_fits(unsigned long long bytes) {
    if (bytes > (SIZE_MAX - 1u) / 2u - BLOOM_HDR_BYTES) {
        bloom_error(XERR_RANGE, "bloom: filter exceeds the addressable serialized size");
        return 0;
    }
    return 1;
}

static unsigned int bloom_optimal_hashes(unsigned long long nbits, unsigned long long n) {
    double k = (double)nbits / (double)n * log(2.0);
    unsigned int hashes = (unsigned int)(k + 0.5);
    if (hashes < 1) hashes = 1;
    if (hashes > 32) hashes = 32;
    return hashes;
}

void *xbloom_create(long long capacity, double fp_rate) {
    XBloom *b;
    unsigned long long nbits, num_blocks;
    size_t total_bytes;

    nbits = bloom_optimal_bits(capacity, fp_rate);
    if (!nbits) return NULL;
    num_blocks = (nbits + BLOOM_BLOCK_BITS - 1) / BLOOM_BLOCK_BITS;
    if (!bloom_body_fits(num_blocks * BLOOM_BLOCK_BYTES)) return NULL;

    b = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!b) {
        bloom_error(XERR_MEMORY, "bloom: out of memory");
        return NULL;
    }

    total_bytes = (size_t)num_blocks * BLOOM_BLOCK_BYTES;
    b->blocks = (unsigned long long *)xrtCalloc(1, total_bytes);
    if (!b->blocks) {
        xrtFree(b);
        bloom_error(XERR_MEMORY, "bloom: out of memory for bit array");
        return NULL;
    }
    b->num_blocks = num_blocks;
    b->nbits = num_blocks * BLOOM_BLOCK_BITS;
    b->nhashes = bloom_optimal_hashes(b->nbits, (unsigned long long)capacity);
    b->count = 0;
    b->capacity = (unsigned long long)capacity;
    return b;
}

void xbloom_destroy(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    if (b) {
        xrtFree(b->blocks);
        xrtFree(b);
    }
}

void xbloom_add(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long base, step, pos;
    unsigned int i;

    if (!b || !data) return;
    if (b->count == LLONG_MAX) { bloom_error(XERR_RANGE, "bloom: add count overflow"); return; }
    if (len < 0) len = (long long)strlen(data);

    base = xrtHash64((const void *)data, (size_t)len);
    step = xrtHash64Seed((const void *)data, (size_t)len,
        0x9e3779b97f4a7c15ULL) | 1ULL;
    for (i = 0; i < b->nhashes; i++) {
        pos = (base + (unsigned long long)i * step) % b->nbits;
        b->blocks[pos >> 6] |= (1ULL << (pos & 63u));
    }
    b->count++;
}

int xbloom_contains(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long base, step, pos, present;
    unsigned int i;

    if (!b || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    base = xrtHash64((const void *)data, (size_t)len);
    step = xrtHash64Seed((const void *)data, (size_t)len,
        0x9e3779b97f4a7c15ULL) | 1ULL;
    present = 1ULL;
    for (i = 0; i < b->nhashes; i++) {
        pos = (base + (unsigned long long)i * step) % b->nbits;
        present &= (b->blocks[pos >> 6] >> (pos & 63u)) & 1ULL;
    }
    return (int)present;
}

void xbloom_clear(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    if (!b) return;
    memset(b->blocks, 0, (size_t)b->num_blocks * BLOOM_BLOCK_BYTES);
    b->count = 0;
}

long long xbloom_count(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    return b ? (long long)b->count : 0;
}

long long xbloom_bits(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    return b ? (long long)b->nbits : 0;
}

int xbloom_hashes(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    return b ? (int)b->nhashes : 0;
}

/* Number of bits currently set in the bit array. */
static unsigned long long xbloom_set_bits(XBloom *b) {
    unsigned long long total = 0, nwords, i;
    nwords = b->num_blocks * BLOOM_BLOCK_WORDS;
    for (i = 0; i < nwords; i++) total += bloom_popcount64(b->blocks[i]);
    return total;
}

/* Current false-positive probability: (setBits / nbits) ^ k. */
double xbloom_estimated_fpp(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long set;
    double ratio;
    if (!b) return 1.0;
    set = xbloom_set_bits(b);
    if (set == 0) return 0.0;
    ratio = (double)set / (double)b->nbits;
    return pow(ratio, (double)b->nhashes);
}

/* Estimated number of DISTINCT elements from the fill ratio:
 *   n* = -(m / k) * ln(1 - setBits / m) */
long long xbloom_estimated_count(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long set;
    double ratio, est;
    if (!b) return 0;
    set = xbloom_set_bits(b);
    if (set == 0) return 0;
    if (set >= b->nbits) return (long long)b->capacity;  /* saturated */
    ratio = (double)set / (double)b->nbits;
    est = -((double)b->nbits / (double)b->nhashes) * log(1.0 - ratio);
    return (long long)(est + 0.5);
}

/* In-place union: this |= other. Both filters must share dimensions.
 * Returns 1 on success, 0 if incompatible or null. */
int xbloom_union(void *aptr, void *bptr) {
    XBloom *a = (XBloom *)aptr, *b = (XBloom *)bptr;
    unsigned long long nwords, i;
    if (!a || !b) return 0;
    if (a->num_blocks != b->num_blocks || a->nhashes != b->nhashes) return 0;
    nwords = a->num_blocks * BLOOM_BLOCK_WORDS;
    for (i = 0; i < nwords; i++) a->blocks[i] |= b->blocks[i];
    return 1;
}

/* Serialize to a hex string (header + bit array). Returns "" on error. */
const char *xbloom_serialize(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long body_bytes, total_bin, i;
    unsigned char hdr[BLOOM_HDR_BYTES];
    const unsigned char *body;
    char *hex, *p;
    if (!b) { bloom_error(XERR_STATE, "bloom: filter is destroyed"); return ""; }
    body_bytes = b->num_blocks * BLOOM_BLOCK_BYTES;
    total_bin = BLOOM_HDR_BYTES + body_bytes;
    hex = (char *)xrtTemp((size_t)(total_bin * 2) + 1);
    if (!hex) { bloom_error(XERR_MEMORY, "bloom: out of memory"); return ""; }
    bloom_put_u32(hdr + 0, BLOOM_MAGIC);
    bloom_put_u32(hdr + 4, BLOOM_VERSION);
    bloom_put_u32(hdr + 8, b->nhashes);
    bloom_put_u32(hdr + 12, 0u);
    bloom_put_u64(hdr + 16, b->num_blocks);
    bloom_put_u64(hdr + 24, b->count);
    bloom_put_u64(hdr + 32, b->capacity);
    p = hex;
    for (i = 0; i < BLOOM_HDR_BYTES; i++) { bloom_hex_byte(p, hdr[i]); p += 2; }
    body = (const unsigned char *)b->blocks;
    for (i = 0; i < body_bytes; i++) { bloom_hex_byte(p, body[i]); p += 2; }
    *p = '\0';
    return hex;
}

/* Reconstruct a filter from a hex string produced by xbloom_serialize.
 * Returns NULL and sets error on invalid input. */
void *xbloom_deserialize(const char *hex) {
    unsigned char *bin;
    size_t total_bin, body_bytes;
    unsigned int magic, version, nhashes;
    unsigned long long num_blocks, count, capacity;
    XBloom *b;

    bin = bloom_hex_decode(hex, &total_bin);
    if (!bin) return NULL;
    if (total_bin < BLOOM_HDR_BYTES) {
        xrtFree(bin); bloom_error(XERR_VALUE, "bloom: truncated data"); return NULL;
    }
    magic = bloom_get_u32(bin + 0);
    version = bloom_get_u32(bin + 4);
    nhashes = bloom_get_u32(bin + 8);
    num_blocks = bloom_get_u64(bin + 16);
    count = bloom_get_u64(bin + 24);
    capacity = bloom_get_u64(bin + 32);
    if (magic != BLOOM_MAGIC) { xrtFree(bin); bloom_error(XERR_VALUE, "bloom: bad magic"); return NULL; }
    if (version != BLOOM_VERSION) { xrtFree(bin); bloom_error(XERR_VALUE, "bloom: unsupported version"); return NULL; }
    if (num_blocks < 1 || num_blocks > LLONG_MAX / BLOOM_BLOCK_BITS ||
        nhashes < 1 || nhashes > 32 || capacity == 0 || capacity > LLONG_MAX ||
        count > LLONG_MAX || bloom_get_u32(bin + 12) != 0) {
        xrtFree(bin); bloom_error(XERR_VALUE, "bloom: corrupt data"); return NULL;
    }
    body_bytes = total_bin - BLOOM_HDR_BYTES;
    /* 以实际输入长度校验声明尺寸，不能先乘不可信的 num_blocks。 */
    if (body_bytes % BLOOM_BLOCK_BYTES != 0 || num_blocks != body_bytes / BLOOM_BLOCK_BYTES) {
        xrtFree(bin); bloom_error(XERR_VALUE, "bloom: size mismatch"); return NULL;
    }
    if (!bloom_body_fits(body_bytes)) { xrtFree(bin); return NULL; }
    b = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!b) { xrtFree(bin); bloom_error(XERR_MEMORY, "bloom: out of memory"); return NULL; }
    b->blocks = (unsigned long long *)xrtCalloc(1, body_bytes);
    if (!b->blocks) { xrtFree(bin); xrtFree(b); bloom_error(XERR_MEMORY, "bloom: out of memory"); return NULL; }
    memcpy(b->blocks, bin + BLOOM_HDR_BYTES, body_bytes);
    b->num_blocks = num_blocks;
    b->nbits = num_blocks * BLOOM_BLOCK_BITS;
    b->nhashes = nhashes;
    b->count = count;
    b->capacity = capacity;
    xrtFree(bin);
    return b;
}

long long xbloom_capacity(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    return b ? (long long)b->capacity : 0;
}

/* Deep copy: independent bit array, identical contents. */
void *xbloom_clone(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    XBloom *c;
    size_t body_bytes;
    if (!b) { bloom_error(XERR_STATE, "bloom: filter is destroyed"); return NULL; }
    c = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!c) { bloom_error(XERR_MEMORY, "bloom: out of memory"); return NULL; }
    body_bytes = (size_t)b->num_blocks * BLOOM_BLOCK_BYTES;
    c->blocks = (unsigned long long *)xrtCalloc(1, body_bytes);
    if (!c->blocks) { xrtFree(c); bloom_error(XERR_MEMORY, "bloom: out of memory"); return NULL; }
    memcpy(c->blocks, b->blocks, body_bytes);
    c->num_blocks = b->num_blocks;
    c->nbits = b->nbits;
    c->nhashes = b->nhashes;
    c->count = b->count;
    c->capacity = b->capacity;
    return c;
}

/* Adopt an existing handle (identity); used to wrap clone results. */
void *xbloom_adopt(void *ptr) { return ptr; }

/* ------------------------------------------------------------------ */
/* Counting Bloom Filter (supports removal; 4-bit packed counters)     */
/* Single rapidhash digest split into base (low 32) + odd step (high). */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned char *counters;   /* 4-bit counters packed, 2 per byte */
    unsigned long long nslots;
    unsigned int nhashes;
    unsigned long long count;
    unsigned long long capacity;
} XCountingBloom;

void *xcounting_bloom_create(long long capacity, double fp_rate) {
    XCountingBloom *cb;
    unsigned long long nbits, nslots, nbytes;

    nbits = bloom_optimal_bits(capacity, fp_rate);
    if (!nbits) return NULL;
    nslots = nbits;
    nbytes = (nslots + 1) / 2;   /* 4-bit counters: 2 slots per byte */
    if (!bloom_body_fits(nbytes)) return NULL;

    cb = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!cb) {
        bloom_error(XERR_MEMORY, "counting bloom: out of memory");
        return NULL;
    }
    cb->counters = (unsigned char *)xrtCalloc(1, (size_t)nbytes);
    if (!cb->counters) {
        xrtFree(cb);
        bloom_error(XERR_MEMORY, "counting bloom: out of memory for counters");
        return NULL;
    }
    cb->nslots = nslots;
    cb->nhashes = bloom_optimal_hashes(nbits, (unsigned long long)capacity);
    cb->count = 0;
    cb->capacity = (unsigned long long)capacity;
    return cb;
}

void xcounting_bloom_destroy(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    if (cb) {
        xrtFree(cb->counters);
        xrtFree(cb);
    }
}

static unsigned char cb_get_counter(XCountingBloom *cb, unsigned long long idx) {
    unsigned long long byte_idx = idx / 2;
    if (idx % 2 == 0) {
        return cb->counters[byte_idx] & 0x0F;
    } else {
        return (cb->counters[byte_idx] >> 4) & 0x0F;
    }
}

static void cb_inc_counter(XCountingBloom *cb, unsigned long long idx) {
    unsigned long long byte_idx = idx / 2;
    unsigned char val;
    if (idx % 2 == 0) {
        val = cb->counters[byte_idx] & 0x0F;
        if (val < 15) {
            cb->counters[byte_idx] = (cb->counters[byte_idx] & 0xF0) | (val + 1);
        }
    } else {
        val = (cb->counters[byte_idx] >> 4) & 0x0F;
        if (val < 15) {
            cb->counters[byte_idx] = (cb->counters[byte_idx] & 0x0F) | ((val + 1) << 4);
        }
    }
}

static void cb_dec_counter(XCountingBloom *cb, unsigned long long idx) {
    unsigned long long byte_idx = idx / 2;
    unsigned char val;
    if (idx % 2 == 0) {
        val = cb->counters[byte_idx] & 0x0F;
        if (val > 0) {
            cb->counters[byte_idx] = (cb->counters[byte_idx] & 0xF0) | (val - 1);
        }
    } else {
        val = (cb->counters[byte_idx] >> 4) & 0x0F;
        if (val > 0) {
            cb->counters[byte_idx] = (cb->counters[byte_idx] & 0x0F) | ((val - 1) << 4);
        }
    }
}

/* Kirsch-Mitzenmacher double hashing over nslots from one digest:
 * idx_i = (base + i * step) % nslots, base = low 32 bits, step = odd high 32. */
void xcounting_bloom_add(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h64, i, idx;
    unsigned int base, step;

    if (!cb || !data) return;
    if (cb->count == LLONG_MAX) { bloom_error(XERR_RANGE, "counting bloom: add count overflow"); return; }
    if (len < 0) len = (long long)strlen(data);

    h64 = xrtHash64((const void *)data, (size_t)len);
    base = (unsigned int)(h64 & 0xFFFFFFFFULL);
    step = ((unsigned int)(h64 >> 32)) | 1u;

    for (i = 0; i < cb->nhashes; i++) {
        idx = ((unsigned long long)base + i * (unsigned long long)step) % cb->nslots;
        cb_inc_counter(cb, idx);
    }
    cb->count++;
}

int xcounting_bloom_contains(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h64, i, idx;
    unsigned int base, step;

    if (!cb || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    h64 = xrtHash64((const void *)data, (size_t)len);
    base = (unsigned int)(h64 & 0xFFFFFFFFULL);
    step = ((unsigned int)(h64 >> 32)) | 1u;

    for (i = 0; i < cb->nhashes; i++) {
        idx = ((unsigned long long)base + i * (unsigned long long)step) % cb->nslots;
        if (cb_get_counter(cb, idx) == 0) {
            return 0;
        }
    }
    return 1;
}

void xcounting_bloom_remove(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h64, i, idx;
    unsigned int base, step;

    if (!cb || !data) return;
    if (len < 0) len = (long long)strlen(data);

    h64 = xrtHash64((const void *)data, (size_t)len);
    base = (unsigned int)(h64 & 0xFFFFFFFFULL);
    step = ((unsigned int)(h64 >> 32)) | 1u;

    /* Only remove if present (single hash, inline presence check). */
    for (i = 0; i < cb->nhashes; i++) {
        idx = ((unsigned long long)base + i * (unsigned long long)step) % cb->nslots;
        if (cb_get_counter(cb, idx) == 0) return;
    }
    for (i = 0; i < cb->nhashes; i++) {
        idx = ((unsigned long long)base + i * (unsigned long long)step) % cb->nslots;
        cb_dec_counter(cb, idx);
    }
    if (cb->count > 0) cb->count--;
}

void xcounting_bloom_clear(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    if (!cb) return;
    memset(cb->counters, 0, (size_t)(cb->nslots + 1) / 2);
    cb->count = 0;
}

long long xcounting_bloom_count(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    return cb ? (long long)cb->count : 0;
}

long long xcounting_bloom_slots(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    return cb ? (long long)cb->nslots : 0;
}

int xcounting_bloom_hashes(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    return cb ? (int)cb->nhashes : 0;
}

static void cb_set_counter(XCountingBloom *cb, unsigned long long idx, unsigned char val) {
    unsigned long long byte_idx = idx / 2;
    if (val > 15) val = 15;
    if (idx % 2 == 0) {
        cb->counters[byte_idx] = (cb->counters[byte_idx] & 0xF0) | (val & 0x0F);
    } else {
        cb->counters[byte_idx] = (cb->counters[byte_idx] & 0x0F) | ((val & 0x0F) << 4);
    }
}

/* Number of slots whose counter is non-zero. */
static unsigned long long cb_nonzero_slots(XCountingBloom *cb) {
    unsigned long long i, n = 0;
    for (i = 0; i < cb->nslots; i++) {
        if (cb_get_counter(cb, i) > 0) n++;
    }
    return n;
}

/* Current false-positive probability: (nonzero / nslots) ^ k. */
double xcounting_bloom_estimated_fpp(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long nz;
    double ratio;
    if (!cb) return 1.0;
    nz = cb_nonzero_slots(cb);
    if (nz == 0) return 0.0;
    ratio = (double)nz / (double)cb->nslots;
    return pow(ratio, (double)cb->nhashes);
}

/* Estimated distinct elements from the non-zero-slot fill ratio. */
long long xcounting_bloom_estimated_count(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long nz;
    double ratio, est;
    if (!cb) return 0;
    nz = cb_nonzero_slots(cb);
    if (nz == 0) return 0;
    if (nz >= cb->nslots) return (long long)cb->capacity;  /* saturated */
    ratio = (double)nz / (double)cb->nslots;
    est = -((double)cb->nslots / (double)cb->nhashes) * log(1.0 - ratio);
    return (long long)(est + 0.5);
}

/* In-place union of counting filters: per-slot counter max.
 * Returns 1 on success, 0 if incompatible or null. */
int xcounting_bloom_union(void *aptr, void *bptr) {
    XCountingBloom *a = (XCountingBloom *)aptr, *b = (XCountingBloom *)bptr;
    unsigned long long i;
    unsigned char va, vb;
    if (!a || !b) return 0;
    if (a->nslots != b->nslots || a->nhashes != b->nhashes) return 0;
    for (i = 0; i < a->nslots; i++) {
        va = cb_get_counter(a, i);
        vb = cb_get_counter(b, i);
        if (vb > va) cb_set_counter(a, i, vb);
    }
    return 1;
}

/* Serialize counting filter to a hex string. Returns "" on error. */
const char *xcounting_bloom_serialize(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long body_bytes, total_bin, i;
    unsigned char hdr[BLOOM_HDR_BYTES];
    const unsigned char *body;
    char *hex, *p;
    if (!cb) { bloom_error(XERR_STATE, "counting bloom: filter is destroyed"); return ""; }
    body_bytes = (cb->nslots + 1) / 2;
    total_bin = BLOOM_HDR_BYTES + body_bytes;
    hex = (char *)xrtTemp((size_t)(total_bin * 2) + 1);
    if (!hex) { bloom_error(XERR_MEMORY, "counting bloom: out of memory"); return ""; }
    bloom_put_u32(hdr + 0, BLOOM_CB_MAGIC);
    bloom_put_u32(hdr + 4, BLOOM_CB_VERSION);
    bloom_put_u32(hdr + 8, cb->nhashes);
    bloom_put_u32(hdr + 12, 0u);
    bloom_put_u64(hdr + 16, cb->nslots);
    bloom_put_u64(hdr + 24, cb->count);
    bloom_put_u64(hdr + 32, cb->capacity);
    p = hex;
    for (i = 0; i < BLOOM_HDR_BYTES; i++) { bloom_hex_byte(p, hdr[i]); p += 2; }
    body = (const unsigned char *)cb->counters;
    for (i = 0; i < body_bytes; i++) { bloom_hex_byte(p, body[i]); p += 2; }
    *p = '\0';
    return hex;
}

/* Reconstruct a counting filter from a hex string. Returns NULL on error. */
void *xcounting_bloom_deserialize(const char *hex) {
    unsigned char *bin;
    size_t total_bin, body_bytes;
    unsigned int magic, version, nhashes;
    unsigned long long nslots, count, capacity;
    XCountingBloom *cb;

    bin = bloom_hex_decode(hex, &total_bin);
    if (!bin) return NULL;
    if (total_bin < BLOOM_HDR_BYTES) {
        xrtFree(bin); bloom_error(XERR_VALUE, "counting bloom: truncated data"); return NULL;
    }
    magic = bloom_get_u32(bin + 0);
    version = bloom_get_u32(bin + 4);
    nhashes = bloom_get_u32(bin + 8);
    nslots = bloom_get_u64(bin + 16);
    count = bloom_get_u64(bin + 24);
    capacity = bloom_get_u64(bin + 32);
    if (magic != BLOOM_CB_MAGIC) { xrtFree(bin); bloom_error(XERR_VALUE, "counting bloom: bad magic"); return NULL; }
    if (version != BLOOM_CB_VERSION) { xrtFree(bin); bloom_error(XERR_VALUE, "counting bloom: unsupported version"); return NULL; }
    if (nslots < 1 || nslots > LLONG_MAX || nhashes < 1 || nhashes > 32 ||
        capacity == 0 || capacity > LLONG_MAX || count > LLONG_MAX ||
        bloom_get_u32(bin + 12) != 0) {
        xrtFree(bin); bloom_error(XERR_VALUE, "counting bloom: corrupt data"); return NULL;
    }
    body_bytes = total_bin - BLOOM_HDR_BYTES;
    if ((nslots + 1) / 2 != body_bytes) {
        xrtFree(bin); bloom_error(XERR_VALUE, "counting bloom: size mismatch"); return NULL;
    }
    if (!bloom_body_fits(body_bytes)) { xrtFree(bin); return NULL; }
    cb = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!cb) { xrtFree(bin); bloom_error(XERR_MEMORY, "counting bloom: out of memory"); return NULL; }
    cb->counters = (unsigned char *)xrtCalloc(1, body_bytes ? body_bytes : 1);
    if (!cb->counters) { xrtFree(bin); xrtFree(cb); bloom_error(XERR_MEMORY, "counting bloom: out of memory"); return NULL; }
    memcpy(cb->counters, bin + BLOOM_HDR_BYTES, body_bytes);
    cb->nslots = nslots;
    cb->nhashes = nhashes;
    cb->count = count;
    cb->capacity = capacity;
    xrtFree(bin);
    return cb;
}

long long xcounting_bloom_capacity(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    return cb ? (long long)cb->capacity : 0;
}

/* Deep copy: independent counter array, identical contents. */
void *xcounting_bloom_clone(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    XCountingBloom *c;
    size_t nbytes;
    if (!cb) { bloom_error(XERR_STATE, "counting bloom: filter is destroyed"); return NULL; }
    c = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!c) { bloom_error(XERR_MEMORY, "counting bloom: out of memory"); return NULL; }
    nbytes = (size_t)(cb->nslots + 1) / 2;
    c->counters = (unsigned char *)xrtCalloc(1, nbytes ? nbytes : 1);
    if (!c->counters) { xrtFree(c); bloom_error(XERR_MEMORY, "counting bloom: out of memory"); return NULL; }
    memcpy(c->counters, cb->counters, nbytes);
    c->nslots = cb->nslots;
    c->nhashes = cb->nhashes;
    c->count = cb->count;
    c->capacity = cb->capacity;
    return c;
}

/* Adopt an existing handle (identity); used to wrap clone results. */
void *xcounting_bloom_adopt(void *ptr) { return ptr; }
