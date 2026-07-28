/* argon2_native.c - Simplified Argon2-like password hashing (PBKDF2-style for TCC) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Simplified password hashing using iterated SHA-256-like construction.
 * Not a full Argon2 implementation (which requires large memory blocks),
 * but provides a compatible API with salt, iterations, and hash output.
 */

static char hash_buf[256];
static char err_buf[256];

const char *xargon2_error(void) { return err_buf; }

/* Simple hash function (FNV-1a based, iterated) */
static unsigned long long fnv1a(const unsigned char *data, int len, unsigned long long seed) {
    unsigned long long h = seed ^ 14695981039346656037ULL;
    int i;
    for (i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Derive a hash from password + salt with iterations */
static void derive_hash(const char *password, const char *salt, int iterations,
                        unsigned char *out, int out_len) {
    unsigned long long state[4];
    int plen = (int)strlen(password);
    int slen = (int)strlen(salt);
    int i, j;

    /* Initialize state from password and salt */
    state[0] = fnv1a((const unsigned char *)password, plen, 0x12345678ULL);
    state[1] = fnv1a((const unsigned char *)salt, slen, 0x9ABCDEF0ULL);
    state[2] = fnv1a((const unsigned char *)password, plen, state[1]);
    state[3] = fnv1a((const unsigned char *)salt, slen, state[0]);

    /* Iterate */
    for (i = 0; i < iterations; i++) {
        state[0] = fnv1a((unsigned char *)state, 32, state[3]);
        state[1] = fnv1a((unsigned char *)state, 32, state[0] ^ (unsigned long long)i);
        state[2] ^= state[0] + state[1];
        state[3] = fnv1a((unsigned char *)(state + 2), 16, state[1]);
    }

    /* Generate output bytes */
    for (j = 0; j < out_len; j++) {
        if (j % 8 == 0 && j > 0) {
            state[0] = fnv1a((unsigned char *)state, 32, state[2] ^ (unsigned long long)j);
        }
        out[j] = (unsigned char)(state[j / 8 % 4] >> ((j % 8) * 8));
    }
}

/* Hash password, return hex string */
const char *xargon2_hash(const char *password, const char *salt, long long iterations, long long hash_len) {
    unsigned char raw[64];
    int hlen = (int)hash_len;
    if (hlen > 64) hlen = 64;
    if (hlen < 16) hlen = 16;
    if (iterations < 1) iterations = 1;
    if (iterations > 1000000) iterations = 1000000;

    derive_hash(password, salt, (int)iterations, raw, hlen);

    int i;
    for (i = 0; i < hlen; i++)
        sprintf(hash_buf + i * 2, "%02x", raw[i]);
    hash_buf[hlen * 2] = '\0';
    return hash_buf;
}

/* Verify password against hash */
long long xargon2_verify(const char *password, const char *salt, long long iterations,
                         long long hash_len, const char *expected) {
    const char *computed = xargon2_hash(password, salt, iterations, hash_len);
    return strcmp(computed, expected) == 0 ? 1 : 0;
}

/* Generate a simple salt (hex string from seed) */
const char *xargon2_gen_salt(long long seed, long long len) {
    static char salt_buf[128];
    int slen = (int)len;
    if (slen > 32) slen = 32;
    if (slen < 8) slen = 8;
    unsigned long long s = (unsigned long long)seed;
    int i;
    for (i = 0; i < slen; i++) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        sprintf(salt_buf + i * 2, "%02x", (unsigned char)(s >> 33));
    }
    salt_buf[slen * 2] = '\0';
    return salt_buf;
}

/* Get recommended iterations */
long long xargon2_recommended_iterations(void) {
    return 10000;
}

/* Get recommended hash length */
long long xargon2_recommended_hashlen(void) {
    return 32;
}
