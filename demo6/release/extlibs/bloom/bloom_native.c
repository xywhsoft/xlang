/*
 * bloom_native.c - High-performance Bloom filter & Counting Bloom filter
 *
 * Performance design (aligned with the aho_corasick zero-allocation ethos):
 *
 *   1. Hashing: delegates to the XRT runtime's champion 64-bit hash,
 *      xrtHash64 (rapidhash). It is SIMD accelerated *inside* the XRT
 *      library (compiled by the host toolchain), so we inherit top-tier
 *      hash speed without writing any SIMD in this TCC-compiled file.
 *      A single digest drives all probes (see below), so the input is
 *      hashed exactly once per add/contains.
 *
 *   2. Blocked Bloom filter: the bit array is partitioned into 512-bit blocks
 *      (= 64 bytes = exactly one CPU cache line), and the block base is
 *      64-byte aligned. Every probe for a given element lands inside a SINGLE
 *      block, so each add/contains costs at most ONE cache miss instead of k
 *      scattered misses. For large filters the random probing cache misses
 *      dominate cost, so this is the highest-value optimization.
 *      (Putze et al., "Cache Efficient Bloom Filters for Shared Memory Machines")
 *
 *   3. Branchless probing: contains() accumulates presence with bitwise AND
 *      across all k probes, with no data-dependent branch in the probe loop.
 *
 *   4. Zero heap allocation on the hot path (add / contains).
 *
 * Single-hash double hashing (Kirsch-Mitzenmacher from one digest):
 *   Each add/contains computes ONE xrtHash64 (rapidhash) digest and splits
 *   its 64 well-mixed bits into independent fields:
 *     - block index  <- high 32 bits  (modulo num_blocks)
 *     - in-block pos <- low 32 bits: low 9 bits = first position,
 *       remaining high bits = odd step (full period modulo 512).
 *   This halves the hashing cost on the hot path versus two separate hash
 *   calls, with negligible FPP impact for a strong 64-bit hash.
 *
 * Semantics notes (for documentation):
 *   - count() reports the number of add() calls (duplicates included);
 *     estimated_count() reports the estimated DISTINCT cardinality.
 *   - Counting filters use 4-bit counters: a single slot saturates at 15,
 *     so adding the same element far more than 15 times is not fully
 *     reversible by remove().
 *   - All operations are single-threaded; no internal synchronization.
 */

#include <string.h>
#include <math.h>
#include "../xlang_extlib_error.h"

/* ------------------------------------------------------------------ */
/* XRT runtime imports                                                 */
/* ------------------------------------------------------------------ */
static void bloom_error(const char *msg) {
    xrtSetErrorInfo(2, "xlang.extlib.bloom", 0, msg);
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
    if (!hex) return NULL;
    hex_len = strlen(hex);
    if ((hex_len % 2) != 0) return NULL;
    n = hex_len / 2;
    buf = (unsigned char *)xrtMalloc(n ? n : 1);
    if (!buf) return NULL;
    for (i = 0; i < n; i++) {
        int hi = bloom_hex_val(hex[i * 2]);
        int lo = bloom_hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) { xrtFree(buf); return NULL; }
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
#define BLOOM_VERSION     1u

/* ------------------------------------------------------------------ */
/* Blocked Bloom filter constants                                      */
/* ------------------------------------------------------------------ */
#define BLOOM_BLOCK_BITS   512u                       /* one cache line of bits */
#define BLOOM_BLOCK_WORDS  8u                         /* 512 / 64 */
#define BLOOM_BLOCK_BYTES  64u                        /* 512 / 8 */
#define BLOOM_POS_MASK     (BLOOM_BLOCK_BITS - 1u)    /* 0x1FF, mod 512 via AND */

/* ------------------------------------------------------------------ */
/* Bloom Filter (blocked, cache-line aligned)                          */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned long long *blocks;    /* 64-byte aligned view into raw_alloc */
    void *raw_alloc;               /* original calloc result, for xrtFree() */
    unsigned long long num_blocks;
    unsigned long long nbits;      /* num_blocks * 512, for reporting */
    unsigned int nhashes;          /* probes per element (k) */
    unsigned long long count;
    unsigned long long capacity;
} XBloom;

static unsigned long long bloom_optimal_bits(unsigned long long n, double fp) {
    double m = -(double)n * log(fp) / (log(2.0) * log(2.0));
    unsigned long long bits = (unsigned long long)(m + 0.5);
    if (bits < BLOOM_BLOCK_BITS) bits = BLOOM_BLOCK_BITS;
    return bits;
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
    unsigned char *raw;
    unsigned long long addr, aligned;
    size_t total_bytes;

    if (capacity <= 0) {
        bloom_error("bloom: capacity must be positive");
        return NULL;
    }
    if (fp_rate <= 0.0 || fp_rate >= 1.0) {
        fp_rate = 0.01;
    }

    nbits = bloom_optimal_bits((unsigned long long)capacity, fp_rate);
    num_blocks = (nbits + BLOOM_BLOCK_BITS - 1) / BLOOM_BLOCK_BITS;
    if (num_blocks < 1) num_blocks = 1;

    b = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!b) {
        bloom_error("bloom: out of memory");
        return NULL;
    }

    /* Over-allocate by one cache line so we can align the block base to
     * 64 bytes, guaranteeing each 512-bit block maps to exactly one
     * cache line and never spans two. */
    total_bytes = (size_t)num_blocks * BLOOM_BLOCK_BYTES;
    raw = (unsigned char *)xrtCalloc(1, total_bytes + BLOOM_BLOCK_BYTES);
    if (!raw) {
        xrtFree(b);
        bloom_error("bloom: out of memory for bit array");
        return NULL;
    }
    addr = (unsigned long long)raw;
    aligned = (addr + (BLOOM_BLOCK_BYTES - 1)) & ~((unsigned long long)(BLOOM_BLOCK_BYTES - 1));
    b->blocks = (unsigned long long *)aligned;
    b->raw_alloc = raw;
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
        xrtFree(b->raw_alloc);
        xrtFree(b);
    }
}

void xbloom_add(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long h64, block_idx;
    unsigned int low, a, step, i, pos;
    unsigned long long *blk;

    if (!b || !data) return;
    if (len < 0) len = (long long)strlen(data);

    h64 = xrtHash64((const void *)data, (size_t)len);

    block_idx = (h64 >> 32) % b->num_blocks;    /* high 32 bits select block */
    blk = b->blocks + block_idx * BLOOM_BLOCK_WORDS;

    low = (unsigned int)(h64 & 0xFFFFFFFFULL);  /* low 32 bits drive probing */
    a = low & BLOOM_POS_MASK;       /* first bit position in the block */
    step = (low >> 9) | 1u;         /* odd step => full period modulo 512 */
    for (i = 0; i < b->nhashes; i++) {
        pos = (a + i * step) & BLOOM_POS_MASK;
        blk[pos >> 6] |= (1ULL << (pos & 63u));
    }
    b->count++;
}

int xbloom_contains(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long h64, block_idx, present;
    unsigned int low, a, step, i, pos;
    unsigned long long *blk;

    if (!b || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    h64 = xrtHash64((const void *)data, (size_t)len);

    block_idx = (h64 >> 32) % b->num_blocks;
    blk = b->blocks + block_idx * BLOOM_BLOCK_WORDS;

    low = (unsigned int)(h64 & 0xFFFFFFFFULL);
    a = low & BLOOM_POS_MASK;
    step = (low >> 9) | 1u;
    present = 1ULL;
    for (i = 0; i < b->nhashes; i++) {
        pos = (a + i * step) & BLOOM_POS_MASK;
        present &= (blk[pos >> 6] >> (pos & 63u)) & 1ULL;
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
    if (!b) { bloom_error("bloom: filter is destroyed"); return ""; }
    body_bytes = b->num_blocks * BLOOM_BLOCK_BYTES;
    total_bin = BLOOM_HDR_BYTES + body_bytes;
    hex = (char *)xrtTemp((size_t)(total_bin * 2) + 1);
    if (!hex) { bloom_error("bloom: out of memory"); return ""; }
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
    unsigned char *raw;
    unsigned long long addr, aligned;

    bin = bloom_hex_decode(hex, &total_bin);
    if (!bin) { bloom_error("bloom: invalid serialized data"); return NULL; }
    if (total_bin < BLOOM_HDR_BYTES) {
        xrtFree(bin); bloom_error("bloom: truncated data"); return NULL;
    }
    magic = bloom_get_u32(bin + 0);
    version = bloom_get_u32(bin + 4);
    nhashes = bloom_get_u32(bin + 8);
    num_blocks = bloom_get_u64(bin + 16);
    count = bloom_get_u64(bin + 24);
    capacity = bloom_get_u64(bin + 32);
    if (magic != BLOOM_MAGIC) { xrtFree(bin); bloom_error("bloom: bad magic"); return NULL; }
    if (version != BLOOM_VERSION) { xrtFree(bin); bloom_error("bloom: unsupported version"); return NULL; }
    if (num_blocks < 1 || nhashes < 1 || nhashes > 32) {
        xrtFree(bin); bloom_error("bloom: corrupt data"); return NULL;
    }
    body_bytes = (size_t)num_blocks * BLOOM_BLOCK_BYTES;
    if (total_bin != BLOOM_HDR_BYTES + body_bytes) {
        xrtFree(bin); bloom_error("bloom: size mismatch"); return NULL;
    }
    b = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!b) { xrtFree(bin); bloom_error("bloom: out of memory"); return NULL; }
    raw = (unsigned char *)xrtCalloc(1, body_bytes + BLOOM_BLOCK_BYTES);
    if (!raw) { xrtFree(bin); xrtFree(b); bloom_error("bloom: out of memory"); return NULL; }
    addr = (unsigned long long)raw;
    aligned = (addr + (BLOOM_BLOCK_BYTES - 1)) & ~((unsigned long long)(BLOOM_BLOCK_BYTES - 1));
    b->blocks = (unsigned long long *)aligned;
    b->raw_alloc = raw;
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
    unsigned char *raw;
    unsigned long long addr, aligned;
    size_t body_bytes;
    if (!b) { bloom_error("bloom: filter is destroyed"); return NULL; }
    c = (XBloom *)xrtCalloc(1, sizeof(XBloom));
    if (!c) { bloom_error("bloom: out of memory"); return NULL; }
    body_bytes = (size_t)b->num_blocks * BLOOM_BLOCK_BYTES;
    raw = (unsigned char *)xrtCalloc(1, body_bytes + BLOOM_BLOCK_BYTES);
    if (!raw) { xrtFree(c); bloom_error("bloom: out of memory"); return NULL; }
    addr = (unsigned long long)raw;
    aligned = (addr + (BLOOM_BLOCK_BYTES - 1)) & ~((unsigned long long)(BLOOM_BLOCK_BYTES - 1));
    c->blocks = (unsigned long long *)aligned;
    c->raw_alloc = raw;
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

    if (capacity <= 0) {
        bloom_error("counting bloom: capacity must be positive");
        return NULL;
    }
    if (fp_rate <= 0.0 || fp_rate >= 1.0) {
        fp_rate = 0.01;
    }

    nbits = bloom_optimal_bits((unsigned long long)capacity, fp_rate);
    nslots = nbits;
    nbytes = (nslots + 1) / 2;   /* 4-bit counters: 2 slots per byte */

    cb = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!cb) {
        bloom_error("counting bloom: out of memory");
        return NULL;
    }
    cb->counters = (unsigned char *)xrtCalloc(1, (size_t)nbytes);
    if (!cb->counters) {
        xrtFree(cb);
        bloom_error("counting bloom: out of memory for counters");
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
    if (!cb) { bloom_error("counting bloom: filter is destroyed"); return ""; }
    body_bytes = (cb->nslots + 1) / 2;
    total_bin = BLOOM_HDR_BYTES + body_bytes;
    hex = (char *)xrtTemp((size_t)(total_bin * 2) + 1);
    if (!hex) { bloom_error("counting bloom: out of memory"); return ""; }
    bloom_put_u32(hdr + 0, BLOOM_CB_MAGIC);
    bloom_put_u32(hdr + 4, BLOOM_VERSION);
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
    if (!bin) { bloom_error("counting bloom: invalid serialized data"); return NULL; }
    if (total_bin < BLOOM_HDR_BYTES) {
        xrtFree(bin); bloom_error("counting bloom: truncated data"); return NULL;
    }
    magic = bloom_get_u32(bin + 0);
    version = bloom_get_u32(bin + 4);
    nhashes = bloom_get_u32(bin + 8);
    nslots = bloom_get_u64(bin + 16);
    count = bloom_get_u64(bin + 24);
    capacity = bloom_get_u64(bin + 32);
    if (magic != BLOOM_CB_MAGIC) { xrtFree(bin); bloom_error("counting bloom: bad magic"); return NULL; }
    if (version != BLOOM_VERSION) { xrtFree(bin); bloom_error("counting bloom: unsupported version"); return NULL; }
    if (nslots < 1 || nhashes < 1 || nhashes > 32) {
        xrtFree(bin); bloom_error("counting bloom: corrupt data"); return NULL;
    }
    body_bytes = (size_t)(nslots + 1) / 2;
    if (total_bin != BLOOM_HDR_BYTES + body_bytes) {
        xrtFree(bin); bloom_error("counting bloom: size mismatch"); return NULL;
    }
    cb = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!cb) { xrtFree(bin); bloom_error("counting bloom: out of memory"); return NULL; }
    cb->counters = (unsigned char *)xrtCalloc(1, body_bytes ? body_bytes : 1);
    if (!cb->counters) { xrtFree(bin); xrtFree(cb); bloom_error("counting bloom: out of memory"); return NULL; }
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
    if (!cb) { bloom_error("counting bloom: filter is destroyed"); return NULL; }
    c = (XCountingBloom *)xrtCalloc(1, sizeof(XCountingBloom));
    if (!c) { bloom_error("counting bloom: out of memory"); return NULL; }
    nbytes = (size_t)(cb->nslots + 1) / 2;
    c->counters = (unsigned char *)xrtCalloc(1, nbytes ? nbytes : 1);
    if (!c->counters) { xrtFree(c); bloom_error("counting bloom: out of memory"); return NULL; }
    memcpy(c->counters, cb->counters, nbytes);
    c->nslots = cb->nslots;
    c->nhashes = cb->nhashes;
    c->count = cb->count;
    c->capacity = cb->capacity;
    return c;
}

/* Adopt an existing handle (identity); used to wrap clone results. */
void *xcounting_bloom_adopt(void *ptr) { return ptr; }
