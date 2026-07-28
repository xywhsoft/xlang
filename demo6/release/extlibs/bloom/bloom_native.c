/*
 * bloom_native.c - Bloom filter and Counting Bloom filter implementation
 *
 * Pure C implementation with no external dependencies.
 * Uses FNV-1a + Kirsch-Mitzenmacher double hashing technique.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Error reporting (XRT convention)                                    */
/* ------------------------------------------------------------------ */

extern void xrtSetError(const char *msg);

static void bloom_error(const char *msg) {
    xrtSetError(msg);
}

/* ------------------------------------------------------------------ */
/* Hashing                                                             */
/* ------------------------------------------------------------------ */

static unsigned long long fnv1a(const unsigned char *data, unsigned long long len) {
    unsigned long long h = 14695981039346656037ULL;
    unsigned long long i;
    for (i = 0; i < len; i++) {
        h ^= (unsigned long long)data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static unsigned long long fnv1a_alt(const unsigned char *data, unsigned long long len) {
    unsigned long long h = 14695981039346656037ULL ^ 0xDEADBEEFCAFEULL;
    unsigned long long i;
    for (i = 0; i < len; i++) {
        h ^= (unsigned long long)data[i];
        h *= 1099511628211ULL;
        h = (h << 13) | (h >> 51);
    }
    return h;
}

/* ------------------------------------------------------------------ */
/* Bloom Filter                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *bits;
    unsigned long long nbits;
    unsigned int nhashes;
    unsigned long long count;
    unsigned long long capacity;
} XBloom;

static unsigned long long bloom_optimal_bits(unsigned long long n, double fp) {
    double m = -(double)n * log(fp) / (log(2.0) * log(2.0));
    unsigned long long bits = (unsigned long long)(m + 0.5);
    if (bits < 64) bits = 64;
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
    unsigned long long nbits, nbytes;

    if (capacity <= 0) {
        bloom_error("bloom: capacity must be positive");
        return NULL;
    }
    if (fp_rate <= 0.0 || fp_rate >= 1.0) {
        fp_rate = 0.01;
    }

    nbits = bloom_optimal_bits((unsigned long long)capacity, fp_rate);
    nbytes = (nbits + 7) / 8;

    b = (XBloom *)calloc(1, sizeof(XBloom));
    if (!b) {
        bloom_error("bloom: out of memory");
        return NULL;
    }
    b->bits = (unsigned char *)calloc(1, nbytes);
    if (!b->bits) {
        free(b);
        bloom_error("bloom: out of memory for bit array");
        return NULL;
    }
    b->nbits = nbits;
    b->nhashes = bloom_optimal_hashes(nbits, (unsigned long long)capacity);
    b->count = 0;
    b->capacity = (unsigned long long)capacity;
    return b;
}

void xbloom_destroy(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    if (b) {
        free(b->bits);
        free(b);
    }
}

void xbloom_add(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long h1, h2, i, idx;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!b || !data) return;
    if (len < 0) len = (long long)strlen(data);

    h1 = fnv1a(bytes, (unsigned long long)len);
    h2 = fnv1a_alt(bytes, (unsigned long long)len);

    for (i = 0; i < b->nhashes; i++) {
        idx = (h1 + i * h2) % b->nbits;
        b->bits[idx / 8] |= (unsigned char)(1 << (idx % 8));
    }
    b->count++;
}

int xbloom_contains(void *ptr, const char *data, long long len) {
    XBloom *b = (XBloom *)ptr;
    unsigned long long h1, h2, i, idx;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!b || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    h1 = fnv1a(bytes, (unsigned long long)len);
    h2 = fnv1a_alt(bytes, (unsigned long long)len);

    for (i = 0; i < b->nhashes; i++) {
        idx = (h1 + i * h2) % b->nbits;
        if (!(b->bits[idx / 8] & (1 << (idx % 8)))) {
            return 0;
        }
    }
    return 1;
}

void xbloom_clear(void *ptr) {
    XBloom *b = (XBloom *)ptr;
    if (!b) return;
    memset(b->bits, 0, (b->nbits + 7) / 8);
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

/* ------------------------------------------------------------------ */
/* Counting Bloom Filter (supports removal)                            */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *counters;   /* 4-bit counters packed */
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
    /* 4-bit counters: 2 slots per byte */
    nbytes = (nslots + 1) / 2;

    cb = (XCountingBloom *)calloc(1, sizeof(XCountingBloom));
    if (!cb) {
        bloom_error("counting bloom: out of memory");
        return NULL;
    }
    cb->counters = (unsigned char *)calloc(1, nbytes);
    if (!cb->counters) {
        free(cb);
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
        free(cb->counters);
        free(cb);
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

void xcounting_bloom_add(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h1, h2, i, idx;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cb || !data) return;
    if (len < 0) len = (long long)strlen(data);

    h1 = fnv1a(bytes, (unsigned long long)len);
    h2 = fnv1a_alt(bytes, (unsigned long long)len);

    for (i = 0; i < cb->nhashes; i++) {
        idx = (h1 + i * h2) % cb->nslots;
        cb_inc_counter(cb, idx);
    }
    cb->count++;
}

int xcounting_bloom_contains(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h1, h2, i, idx;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cb || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    h1 = fnv1a(bytes, (unsigned long long)len);
    h2 = fnv1a_alt(bytes, (unsigned long long)len);

    for (i = 0; i < cb->nhashes; i++) {
        idx = (h1 + i * h2) % cb->nslots;
        if (cb_get_counter(cb, idx) == 0) {
            return 0;
        }
    }
    return 1;
}

void xcounting_bloom_remove(void *ptr, const char *data, long long len) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    unsigned long long h1, h2, i, idx;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cb || !data) return;
    if (len < 0) len = (long long)strlen(data);

    /* Only remove if present */
    if (!xcounting_bloom_contains(ptr, data, len)) return;

    h1 = fnv1a(bytes, (unsigned long long)len);
    h2 = fnv1a_alt(bytes, (unsigned long long)len);

    for (i = 0; i < cb->nhashes; i++) {
        idx = (h1 + i * h2) % cb->nslots;
        cb_dec_counter(cb, idx);
    }
    if (cb->count > 0) cb->count--;
}

void xcounting_bloom_clear(void *ptr) {
    XCountingBloom *cb = (XCountingBloom *)ptr;
    if (!cb) return;
    memset(cb->counters, 0, (cb->nslots + 1) / 2);
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
