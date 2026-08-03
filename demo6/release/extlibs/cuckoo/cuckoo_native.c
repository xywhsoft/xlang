/*
 * cuckoo_native.c - Cuckoo Filter implementation
 *
 * Supports insert, lookup, and delete with space efficiency.
 * Uses 16-bit fingerprints, 4 entries per bucket, partial-key cuckoo hashing.
 */

#include <stdlib.h>
#include <string.h>
extern void xrtSetError(const void *msg, int bFree);
/* ------------------------------------------------------------------ */
/* Hashing                                                             */
/* ------------------------------------------------------------------ */

static unsigned long long cuckoo_hash(const unsigned char *data, unsigned long long len) {
    unsigned long long h = 14695981039346656037ULL;
    unsigned long long i;
    for (i = 0; i < len; i++) {
        h ^= (unsigned long long)data[i];
        h *= 1099511628211ULL;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

/* ------------------------------------------------------------------ */
/* Cuckoo Filter                                                       */
/* ------------------------------------------------------------------ */

#define CUCKOO_ENTRIES_PER_BUCKET 4
#define CUCKOO_MAX_KICKS 500

typedef struct {
    unsigned short *buckets;    /* nbuckets * 4 entries, 16-bit fingerprints */
    unsigned int nbuckets;      /* power of 2 */
    unsigned int bucket_mask;   /* nbuckets - 1 */
    unsigned long long count;
    unsigned long long capacity;
} XCuckoo;

static unsigned short cuckoo_fingerprint(unsigned long long hash) {
    unsigned short fp = (unsigned short)((hash >> 32) & 0xFFFF);
    if (fp == 0) fp = 1;  /* Reserve 0 as empty marker */
    return fp;
}

static unsigned int cuckoo_index1(XCuckoo *cf, unsigned long long hash) {
    return (unsigned int)(hash & (unsigned long long)cf->bucket_mask);
}

static unsigned int cuckoo_index2(XCuckoo *cf, unsigned int i1, unsigned short fp) {
    /* Partial-key cuckoo hashing: i2 = i1 XOR hash(fp) */
    unsigned int fp_hash = (unsigned int)fp * 2654435761u;
    return (i1 ^ fp_hash) & cf->bucket_mask;
}

void *xcuckoo_create(long long capacity) {
    XCuckoo *cf;
    unsigned int nbuckets;
    unsigned long long total_entries;

    if (capacity <= 0) {
        xrtSetError("cuckoo: capacity must be positive", 0);
        return NULL;
    }

    /* Each bucket holds 4 entries; target ~95% load factor */
    nbuckets = (unsigned int)((capacity * 2) / CUCKOO_ENTRIES_PER_BUCKET);
    /* Round up to power of 2 */
    {
        unsigned int p = 1;
        while (p < nbuckets) p <<= 1;
        nbuckets = p;
    }
    if (nbuckets < 4) nbuckets = 4;

    total_entries = (unsigned long long)nbuckets * CUCKOO_ENTRIES_PER_BUCKET;

    cf = (XCuckoo *)calloc(1, sizeof(XCuckoo));
    if (!cf) {
        xrtSetError("cuckoo: out of memory", 0);
        return NULL;
    }
    cf->buckets = (unsigned short *)calloc(total_entries, sizeof(unsigned short));
    if (!cf->buckets) {
        free(cf);
        xrtSetError("cuckoo: out of memory for buckets", 0);
        return NULL;
    }
    cf->nbuckets = nbuckets;
    cf->bucket_mask = nbuckets - 1;
    cf->count = 0;
    cf->capacity = (unsigned long long)capacity;
    return cf;
}

void xcuckoo_destroy(void *ptr) {
    XCuckoo *cf = (XCuckoo *)ptr;
    if (cf) {
        free(cf->buckets);
        free(cf);
    }
}

static int cuckoo_bucket_insert(XCuckoo *cf, unsigned int bucket, unsigned short fp) {
    int i;
    unsigned short *base = cf->buckets + (unsigned long long)bucket * CUCKOO_ENTRIES_PER_BUCKET;
    for (i = 0; i < CUCKOO_ENTRIES_PER_BUCKET; i++) {
        if (base[i] == 0) {
            base[i] = fp;
            return 1;
        }
    }
    return 0;
}

static int cuckoo_bucket_contains(XCuckoo *cf, unsigned int bucket, unsigned short fp) {
    int i;
    unsigned short *base = cf->buckets + (unsigned long long)bucket * CUCKOO_ENTRIES_PER_BUCKET;
    for (i = 0; i < CUCKOO_ENTRIES_PER_BUCKET; i++) {
        if (base[i] == fp) return 1;
    }
    return 0;
}

static int cuckoo_bucket_remove(XCuckoo *cf, unsigned int bucket, unsigned short fp) {
    int i;
    unsigned short *base = cf->buckets + (unsigned long long)bucket * CUCKOO_ENTRIES_PER_BUCKET;
    for (i = 0; i < CUCKOO_ENTRIES_PER_BUCKET; i++) {
        if (base[i] == fp) {
            base[i] = 0;
            return 1;
        }
    }
    return 0;
}

int xcuckoo_add(void *ptr, const char *data, long long len) {
    XCuckoo *cf = (XCuckoo *)ptr;
    unsigned long long hash;
    unsigned short fp;
    unsigned int i1, i2, kick;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cf || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    hash = cuckoo_hash(bytes, (unsigned long long)len);
    fp = cuckoo_fingerprint(hash);
    i1 = cuckoo_index1(cf, hash);

    if (cuckoo_bucket_insert(cf, i1, fp)) {
        cf->count++;
        return 1;
    }

    i2 = cuckoo_index2(cf, i1, fp);
    if (cuckoo_bucket_insert(cf, i2, fp)) {
        cf->count++;
        return 1;
    }

    /* Cuckoo displacement */
    {
        unsigned int idx = i1;
        unsigned short cur_fp = fp;
        /* Simple PRNG for victim selection */
        unsigned long long rng = hash ^ 0x123456789ABCDEFULL;

        for (kick = 0; kick < CUCKOO_MAX_KICKS; kick++) {
            int victim;
            unsigned short *base;
            unsigned short old_fp;

            rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
            victim = (int)((rng >> 33) % CUCKOO_ENTRIES_PER_BUCKET);

            base = cf->buckets + (unsigned long long)idx * CUCKOO_ENTRIES_PER_BUCKET;
            old_fp = base[victim];
            base[victim] = cur_fp;
            cur_fp = old_fp;

            idx = cuckoo_index2(cf, idx, cur_fp);
            if (cuckoo_bucket_insert(cf, idx, cur_fp)) {
                cf->count++;
                return 1;
            }
        }
    }

    xrtSetError("cuckoo: filter is full", 0);
    return 0;
}

int xcuckoo_contains(void *ptr, const char *data, long long len) {
    XCuckoo *cf = (XCuckoo *)ptr;
    unsigned long long hash;
    unsigned short fp;
    unsigned int i1, i2;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cf || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    hash = cuckoo_hash(bytes, (unsigned long long)len);
    fp = cuckoo_fingerprint(hash);
    i1 = cuckoo_index1(cf, hash);
    i2 = cuckoo_index2(cf, i1, fp);

    return cuckoo_bucket_contains(cf, i1, fp) || cuckoo_bucket_contains(cf, i2, fp);
}

int xcuckoo_remove(void *ptr, const char *data, long long len) {
    XCuckoo *cf = (XCuckoo *)ptr;
    unsigned long long hash;
    unsigned short fp;
    unsigned int i1, i2;
    const unsigned char *bytes = (const unsigned char *)data;

    if (!cf || !data) return 0;
    if (len < 0) len = (long long)strlen(data);

    hash = cuckoo_hash(bytes, (unsigned long long)len);
    fp = cuckoo_fingerprint(hash);
    i1 = cuckoo_index1(cf, hash);
    i2 = cuckoo_index2(cf, i1, fp);

    if (cuckoo_bucket_remove(cf, i1, fp)) {
        cf->count--;
        return 1;
    }
    if (cuckoo_bucket_remove(cf, i2, fp)) {
        cf->count--;
        return 1;
    }
    return 0;
}

void xcuckoo_clear(void *ptr) {
    XCuckoo *cf = (XCuckoo *)ptr;
    if (!cf) return;
    memset(cf->buckets, 0,
        (unsigned long long)cf->nbuckets * CUCKOO_ENTRIES_PER_BUCKET * sizeof(unsigned short));
    cf->count = 0;
}

long long xcuckoo_count(void *ptr) {
    XCuckoo *cf = (XCuckoo *)ptr;
    return cf ? (long long)cf->count : 0;
}

long long xcuckoo_buckets(void *ptr) {
    XCuckoo *cf = (XCuckoo *)ptr;
    return cf ? (long long)cf->nbuckets : 0;
}
