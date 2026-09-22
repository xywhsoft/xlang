/*
 * oauth2_native.c - OAuth2 / PKCE (RFC 7636) helpers.
 *
 * buildAuthorizeUrl assembles a fully escaped authorization endpoint
 * URL. makeVerifier emits a 43-character base64url verifier seeded
 * from time and address entropy (demo grade; production callers may
 * pass their own verifier). codeChallenge computes the S256 challenge
 * as base64url(SHA-256(verifier)) with a self-contained SHA-256.
 * parseCallbackQuery extracts code/state/error from a redirect query.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XOA_DOMAIN "xlang.extlib.oauth2"

static void xoa_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XOA_DOMAIN, 0, message);
}

/* ---------------- URL encoding ---------------- */

static int xoa_unreserved(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

static int xoa_url_append(char *out, size_t cap, size_t *pos,
    const char *text, int escape) {
    const char *p;
    static const char *hex = "0123456789ABCDEF";
    for (p = text; *p != '\0'; p++) {
        if (!escape || xoa_unreserved((unsigned char)*p)) {
            if (*pos + 1 >= cap) return 0;
            out[(*pos)++] = *p;
        } else {
            if (*pos + 3 >= cap) return 0;
            out[(*pos)++] = '%';
            out[(*pos)++] = hex[(unsigned char)*p >> 4];
            out[(*pos)++] = hex[(unsigned char)*p & 0xF];
        }
    }
    return 1;
}

const char *xoa_build_authorize_url(const char *baseUrl,
    const char *clientId, const char *redirectUri, const char *scope,
    const char *state, const char *challenge) {
    static char url[2048];
    size_t pos = 0;
    char sep = '?';
    const char *p;
    if (baseUrl == NULL || clientId == NULL || redirectUri == NULL ||
        challenge == NULL) {
        xoa_error("authorize url needs base, client, redirect and challenge");
        url[0] = '\0';
        return url;
    }
    for (p = baseUrl; *p != '\0'; p++) {
        if (*p == '?' || *p == '#') sep = '&';
    }
    if (!xoa_url_append(url, sizeof(url), &pos, baseUrl, 0)) goto fail;
    url[pos++] = sep;
    if (!xoa_url_append(url, sizeof(url), &pos, "response_type=code", 0))
        goto fail;
    url[pos++] = '&';
    if (!xoa_url_append(url, sizeof(url), &pos, "client_id=", 0)) goto fail;
    if (!xoa_url_append(url, sizeof(url), &pos, clientId, 1)) goto fail;
    url[pos++] = '&';
    if (!xoa_url_append(url, sizeof(url), &pos, "redirect_uri=", 0))
        goto fail;
    if (!xoa_url_append(url, sizeof(url), &pos, redirectUri, 1)) goto fail;
    if (scope != NULL && scope[0] != '\0') {
        url[pos++] = '&';
        if (!xoa_url_append(url, sizeof(url), &pos, "scope=", 0)) goto fail;
        if (!xoa_url_append(url, sizeof(url), &pos, scope, 0)) goto fail;
    }
    if (state != NULL && state[0] != '\0') {
        url[pos++] = '&';
        if (!xoa_url_append(url, sizeof(url), &pos, "state=", 0)) goto fail;
        if (!xoa_url_append(url, sizeof(url), &pos, state, 1)) goto fail;
    }
    url[pos++] = '&';
    if (!xoa_url_append(url, sizeof(url), &pos, "code_challenge=", 0))
        goto fail;
    if (!xoa_url_append(url, sizeof(url), &pos, challenge, 0)) goto fail;
    url[pos++] = '&';
    if (!xoa_url_append(url, sizeof(url), &pos,
            "code_challenge_method=S256", 0)) goto fail;
    url[pos] = '\0';
    return url;
fail:
    xoa_error("authorize url is too long");
    url[0] = '\0';
    return url;
}

/* ---------------- SHA-256 (FIPS 180-4) ---------------- */

typedef struct {
    unsigned int state[8];
    unsigned long long bitlen;
    unsigned char data[64];
    unsigned int datalen;
} XoaSha256;

static void xoa_sha_transform(XoaSha256 *ctx, const unsigned char *data) {
    static const unsigned int k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
        0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
        0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
        0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
        0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
        0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    unsigned int m[64];
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int e;
    unsigned int f;
    unsigned int g;
    unsigned int h;
    unsigned int t1;
    unsigned int t2;
    int i;
    int j;
    for (i = 0, j = 0; i < 16; i++, j += 4) {
        m[i] = ((unsigned int)data[j] << 24) |
               ((unsigned int)data[j + 1] << 16) |
               ((unsigned int)data[j + 2] << 8) |
               (unsigned int)data[j + 3];
    }
    for (; i < 64; i++) {
        unsigned int s0 = ((m[i - 15] >> 7) | (m[i - 15] << 25)) ^
                          ((m[i - 15] >> 18) | (m[i - 15] << 14)) ^
                          (m[i - 15] >> 3);
        unsigned int s1 = ((m[i - 2] >> 17) | (m[i - 2] << 15)) ^
                          ((m[i - 2] >> 19) | (m[i - 2] << 13)) ^
                          (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    for (i = 0; i < 64; i++) {
        unsigned int s1 = ((e >> 6) | (e << 26)) ^
                          ((e >> 11) | (e << 21)) ^
                          ((e >> 25) | (e << 7));
        unsigned int ch = (e & f) ^ (~e & g);
        unsigned int s0 = ((a >> 2) | (a << 30)) ^
                          ((a >> 13) | (a << 19)) ^
                          ((a >> 22) | (a << 10));
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + k[i] + m[i];
        t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void xoa_sha_init(XoaSha256 *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void xoa_sha_update(XoaSha256 *ctx, const unsigned char *data,
    size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            xoa_sha_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void xoa_sha_final(XoaSha256 *ctx, unsigned char *out) {
    size_t i = ctx->datalen;
    int j;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        xoa_sha_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += (unsigned long long)ctx->datalen * 8;
    for (j = 0; j < 8; j++) {
        ctx->data[63 - j] = (unsigned char)(ctx->bitlen >> (j * 8));
    }
    xoa_sha_transform(ctx, ctx->data);
    for (j = 0; j < 4; j++) {
        int w;
        for (w = 0; w < 8; w++) {
            out[w * 4 + j] =
                (unsigned char)((ctx->state[w] >> (24 - j * 8)) & 0xFF);
        }
    }
}

/* base64url without padding, input up to 42 bytes */
static const char *xoa_b64url(const unsigned char *in, size_t n) {
    static char out[64];
    static const char *tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t i = 0;
    size_t o = 0;
    while (i < n) {
        unsigned int v = (unsigned int)in[i] << 16;
        int remaining = (int)(n - i);
        if (remaining >= 2) v |= (unsigned int)in[i + 1] << 8;
        if (remaining >= 3) v |= (unsigned int)in[i + 2];
        out[o++] = tbl[(v >> 18) & 0x3F];
        if (o >= 63) break;
        out[o++] = tbl[(v >> 12) & 0x3F];
        if (o >= 63) break;
        out[o++] = remaining >= 2 ? tbl[(v >> 6) & 0x3F] : '\0';
        if (o >= 63) break;
        out[o++] = remaining >= 3 ? tbl[v & 0x3F] : '\0';
        i += 3;
    }
    out[o] = '\0';
    return out;
}

const char *xoa_code_challenge(const char *verifier) {
    XoaSha256 ctx;
    unsigned char digest[32];
    size_t n = verifier != NULL ? strlen(verifier) : 0;
    if (n < 43 || n > 128) {
        xoa_error("verifier must be 43-128 characters");
        return "";
    }
    xoa_sha_init(&ctx);
    xoa_sha_update(&ctx, (const unsigned char *)(verifier ? verifier : ""),
        n);
    xoa_sha_final(&ctx, digest);
    return xoa_b64url(digest, 32);
}

long long xoa_verify_challenge(const char *verifier, const char *challenge) {
    const char *computed = xoa_code_challenge(verifier);
    if (computed[0] == '\0' || challenge == NULL) return 0;
    return strcmp(computed, challenge) == 0;
}

const char *xoa_make_verifier(void) {
    static char verifier[44];
    unsigned char raw[33];
    size_t i;
    unsigned int seed =
        (unsigned int)time(NULL) ^ (unsigned int)(size_t)&verifier;
    for (i = 0; i < sizeof(raw); i++) {
        seed = seed * 1103515245u + 12345u;
        raw[i] = (unsigned char)(seed >> 16);
    }
    {
        const char *encoded = xoa_b64url(raw, 32);
        strncpy(verifier, encoded, 43);
        verifier[43] = '\0';
    }
    return verifier;
}

xvalue *xoa_parse_callback(const char *query) {
    xvalue *result;
    const char *p = query;
    static char key[64];
    static char value[512];
    if (query == NULL) {
        xoa_error("callback query is null");
        return xrtValueNull();
    }
    result = xrtValueObject();
    if (result == NULL) return xrtValueNull();
    while (*p != '\0') {
        size_t klen;
        size_t vlen;
        const char *eq;
        const char *amp;
        while (*p == '&' || *p == '?') p++;
        if (*p == '\0') break;
        eq = strchr(p, '=');
        amp = strchr(p, '&');
        if (amp == NULL) amp = p + strlen(p);
        if (eq == NULL || eq > amp) {
            p = (*amp == '\0') ? amp : amp + 1;
            continue;
        }
        klen = (size_t)(eq - p);
        vlen = (size_t)(amp - eq - 1);
        if (klen < sizeof(key) && vlen < sizeof(value)) {
            memcpy(key, p, klen);
            key[klen] = '\0';
            memcpy(value, eq + 1, vlen);
            value[vlen] = '\0';
            {
                xstrview view;
                str owned = (str)xrtMalloc(vlen + 1);
                xvalue *text = NULL;
                view.Data = key;
                view.Size = klen;
                if (owned != NULL) {
                    memcpy(owned, value, vlen);
                    owned[vlen] = '\0';
                    text = xrtValueStringTake(&owned, vlen);
                }
                xrtValueObjectSetNew(result, view, text);
            }
        }
        p = (*amp == '\0') ? amp : amp + 1;
    }
    return result;
}
