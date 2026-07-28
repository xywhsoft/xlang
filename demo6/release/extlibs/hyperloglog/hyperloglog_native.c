/*
 * hyperloglog_native.c - HyperLogLog cardinality estimator
 *
 * Pure C implementation using 64-bit FNV-1a hash.
 * Supports precision 4-18 (default 14, ~0.81% standard error).
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

extern void xrtSetError(const char *msg);

/* ------------------------------------------------------------------ */
/* Hashing                                                             */
/* ------------------------------------------------------------------ */

static unsigned long long hll_hash(const unsigned char *data, unsigned long long len) {
    unsigned long long h = 14695981039346656037ULL;
    unsigned long long i;
    for (i = 0; i < len; i++) {
        h ^= (unsigned long long)data[i];
        h *= 1099511628211ULL;
    }
    /* Additional mixing for better distribution */
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;
    return h;
}

/* ------------------------------------------------------------------ */
/* HyperLogLog                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *registers;
    unsigned int precision;     /* p: 4..18 */
    unsigned int nregisters;    /* m = 2^p */
    unsigned long long added;   /* total add calls (for reference) */
} XHll;

static int hll_leading_zeros(unsigned long long value, int maxbits) {
    int count = 1;
    int i;
    for (i = maxbits - 1; i >= 0; i--) {
        if (value & (1ULL << i)) {
            break;
        }
        count++;
    }
    return count;
}

void *xhll_create(int precision) {
    XHll *hll;
    unsigned int m;

    if (precision < 4) precision = 4;
    if (precision > 18) precision = 18;

    m = 1u << precision;

    hll = (XHll *)calloc(1, sizeof(XHll));
    if (!hll) {
        xrtSetError("hyperloglog: out of memory");
        return NULL;
    }
    hll->registers = (unsigned char *)calloc(m, 1);
    if (!hll->registers) {
        free(hll);
        xrtSetError("hyperloglog: out of memory for registers");
        return NULL;
    }
    hll->precision = (unsigned int)precision;
    hll->nregisters = m;
    hll->added = 0;
    return hll;
}

void xhll_destroy(void *ptr) {
    XHll *hll = (XHll *)ptr;
    if (hll) {
        free(hll->registers);
        free(hll);
    }
}

void xhll_add(void *ptr, const char *data, long long len) {
    XHll *hll = (XHll *)ptr;
    unsigned long long hash;
    unsigned int idx;
    int rho;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!hll || !data) return;
    if (len < 0) len = (long long)strlen(data);

    hash = hll_hash(bytes, (unsigned long long)len);

    /* Use top p bits as register index */
    idx = (unsigned int)(hash >> (64 - hll->precision));
    /* Count leading zeros in remaining bits */
    rho = hll_leading_zeros(hash << hll->precision, 64 - (int)hll->precision);

    if ((unsigned char)rho > hll->registers[idx]) {
        hll->registers[idx] = (unsigned char)rho;
    }
    hll->added++;
}

long long xhll_count(void *ptr) {
    XHll *hll = (XHll *)ptr;
    double alpha, sum, estimate;
    unsigned int m, i;
    int zeros;

    if (!hll) return 0;

    m = hll->nregisters;

    /* Alpha constant */
    if (m == 16) alpha = 0.673;
    else if (m == 32) alpha = 0.697;
    else if (m == 64) alpha = 0.709;
    else alpha = 0.7213 / (1.0 + 1.079 / (double)m);

    sum = 0.0;
    zeros = 0;
    for (i = 0; i < m; i++) {
        sum += 1.0 / (double)(1ULL << hll->registers[i]);
        if (hll->registers[i] == 0) zeros++;
    }

    estimate = alpha * (double)m * (double)m / sum;

    /* Small range correction (linear counting) */
    if (estimate <= 2.5 * (double)m && zeros > 0) {
        estimate = (double)m * log((double)m / (double)zeros);
    }

    /* Large range correction (for 64-bit hash, threshold is 2^63 / 30) */
    /* Not needed in practice for reasonable cardinalities */

    return (long long)(estimate + 0.5);
}

void xhll_clear(void *ptr) {
    XHll *hll = (XHll *)ptr;
    if (!hll) return;
    memset(hll->registers, 0, hll->nregisters);
    hll->added = 0;
}

int xhll_merge(void *dst_ptr, void *src_ptr) {
    XHll *dst = (XHll *)dst_ptr;
    XHll *src = (XHll *)src_ptr;
    unsigned int i;

    if (!dst || !src) return 0;
    if (dst->precision != src->precision) {
        xrtSetError("hyperloglog: cannot merge HLLs with different precision");
        return 0;
    }

    for (i = 0; i < dst->nregisters; i++) {
        if (src->registers[i] > dst->registers[i]) {
            dst->registers[i] = src->registers[i];
        }
    }
    return 1;
}

int xhll_precision(void *ptr) {
    XHll *hll = (XHll *)ptr;
    return hll ? (int)hll->precision : 0;
}

long long xhll_added(void *ptr) {
    XHll *hll = (XHll *)ptr;
    return hll ? (long long)hll->added : 0;
}
