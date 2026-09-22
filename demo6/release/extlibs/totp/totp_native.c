/*
 * totp_native.c - TOTP/HOTP (RFC 6238 / RFC 4226) with HMAC-SHA1
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static char totp_buf[64];

/* ------------------------------------------------------------------ */
/* Minimal SHA-1 implementation                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned int state[5];
    unsigned long long count;
    unsigned char buffer[64];
} SHA1_CTX;

#define SHA1_ROL(v, b) (((v) << (b)) | ((v) >> (32 - (b))))

static void sha1_transform(unsigned int state[5], const unsigned char buf[64]) {
    unsigned int a, b, c, d, e, w[80];
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (buf[i*4]<<24)|(buf[i*4+1]<<16)|(buf[i*4+2]<<8)|buf[i*4+3];
    for (i = 16; i < 80; i++)
        w[i] = SHA1_ROL(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);
    a=state[0]; b=state[1]; c=state[2]; d=state[3]; e=state[4];
    for (i = 0; i < 80; i++) {
        unsigned int f, k, tmp;
        if (i < 20) { f = (b&c)|((~b)&d); k = 0x5A827999; }
        else if (i < 40) { f = b^c^d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b&c)|(b&d)|(c&d); k = 0x8F1BBCDC; }
        else { f = b^c^d; k = 0xCA62C1D6; }
        tmp = SHA1_ROL(a,5) + f + e + k + w[i];
        e=d; d=c; c=SHA1_ROL(b,30); b=a; a=tmp;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d; state[4]+=e;
}

static void sha1_init(SHA1_CTX *ctx) {
    ctx->state[0]=0x67452301; ctx->state[1]=0xEFCDAB89;
    ctx->state[2]=0x98BADCFE; ctx->state[3]=0x10325476;
    ctx->state[4]=0xC3D2E1F0; ctx->count=0;
}

static void sha1_update(SHA1_CTX *ctx, const unsigned char *data, unsigned long len) {
    unsigned long i, idx;
    idx = (unsigned long)(ctx->count & 63);
    ctx->count += len;
    for (i = 0; i < len; i++) {
        ctx->buffer[idx++] = data[i];
        if (idx == 64) { sha1_transform(ctx->state, ctx->buffer); idx = 0; }
    }
}

static void sha1_final(SHA1_CTX *ctx, unsigned char digest[20]) {
    unsigned long idx = (unsigned long)(ctx->count & 63);
    unsigned long long bits = ctx->count * 8;
    int i;
    ctx->buffer[idx++] = 0x80;
    if (idx > 56) { memset(ctx->buffer+idx, 0, 64-idx); sha1_transform(ctx->state, ctx->buffer); idx = 0; }
    memset(ctx->buffer+idx, 0, 56-idx);
    for (i = 0; i < 8; i++) ctx->buffer[56+i] = (unsigned char)(bits >> (56 - i*8));
    sha1_transform(ctx->state, ctx->buffer);
    for (i = 0; i < 20; i++) digest[i] = (unsigned char)(ctx->state[i/4] >> (24 - (i%4)*8));
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA1                                                           */
/* ------------------------------------------------------------------ */
static void hmac_sha1(const unsigned char *key, int key_len,
                      const unsigned char *msg, int msg_len,
                      unsigned char out[20]) {
    SHA1_CTX ctx;
    unsigned char k_pad[64], tk[20];
    int i;
    if (key_len > 64) {
        sha1_init(&ctx); sha1_update(&ctx, key, key_len); sha1_final(&ctx, tk);
        key = tk; key_len = 20;
    }
    memset(k_pad, 0x36, 64);
    for (i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    sha1_init(&ctx); sha1_update(&ctx, k_pad, 64); sha1_update(&ctx, msg, msg_len); sha1_final(&ctx, out);
    memset(k_pad, 0x5c, 64);
    for (i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    sha1_init(&ctx); sha1_update(&ctx, k_pad, 64); sha1_update(&ctx, out, 20); sha1_final(&ctx, out);
}

/* ------------------------------------------------------------------ */
/* Base32 decode (RFC 4648)                                            */
/* ------------------------------------------------------------------ */
static int b32_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

static int base32_decode(const char *in, unsigned char *out, int max_out) {
    int bits = 0, val = 0, olen = 0;
    while (*in && *in != '=') {
        int v = b32_val(*in);
        if (v < 0) { in++; continue; }
        val = (val << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (olen < max_out) out[olen++] = (unsigned char)(val >> bits);
        }
        in++;
    }
    return olen;
}

/* ------------------------------------------------------------------ */
/* HOTP / TOTP                                                         */
/* ------------------------------------------------------------------ */
static int hotp_generate(const unsigned char *secret, int secret_len,
                         long long counter, int digits) {
    unsigned char msg[8], hmac[20];
    int offset, code, i;
    for (i = 7; i >= 0; i--) { msg[i] = (unsigned char)(counter & 0xFF); counter >>= 8; }
    hmac_sha1(secret, secret_len, msg, 8, hmac);
    offset = hmac[19] & 0x0F;
    code = ((hmac[offset] & 0x7F) << 24) | (hmac[offset+1] << 16) |
           (hmac[offset+2] << 8) | hmac[offset+3];
    for (i = 0; i < digits; i++) code = code % 1000000000;
    /* compute mod 10^digits */
    { int mod = 1; for (i = 0; i < digits; i++) mod *= 10; code = code % mod; }
    return code;
}

const char *xtotp_hotp(const char *b32_secret, long long counter, int digits) {
    unsigned char secret[128];
    int slen, code;
    if (!b32_secret) return "";
    if (digits < 6) digits = 6;
    if (digits > 9) digits = 9;
    slen = base32_decode(b32_secret, secret, sizeof(secret));
    if (slen <= 0) return "";
    code = hotp_generate(secret, slen, counter, digits);
    snprintf(totp_buf, sizeof(totp_buf), "%0*d", digits, code);
    return totp_buf;
}

const char *xtotp_generate_at(const char *b32_secret, long long timestamp,
                              int time_step, int digits) {
    long long counter;
    if (time_step <= 0) time_step = 30;
    counter = timestamp / time_step;
    return xtotp_hotp(b32_secret, counter, digits);
}

const char *xtotp_generate(const char *b32_secret, int time_step, int digits) {
    long long now = (long long)time(NULL);
    return xtotp_generate_at(b32_secret, now, time_step, digits);
}

int xtotp_verify_at(const char *b32_secret, const char *code,
                    long long timestamp, int time_step, int digits, int window) {
    int w;
    if (time_step <= 0) time_step = 30;
    for (w = -window; w <= window; w++) {
        long long ts = timestamp + w * time_step;
        const char *gen = xtotp_generate_at(b32_secret, ts, time_step, digits);
        if (strcmp(gen, code) == 0) return 1;
    }
    return 0;
}

int xtotp_verify(const char *b32_secret, const char *code, int time_step, int digits, int window) {
    long long now = (long long)time(NULL);
    return xtotp_verify_at(b32_secret, code, now, time_step, digits, window);
}
