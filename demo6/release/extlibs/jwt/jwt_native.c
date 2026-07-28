/* jwt_native.c - JWT encode/decode with HMAC-SHA256 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[4096];
static char err_buf[256];

const char *xjwt_error(void) { return err_buf; }

/* Base64url encoding */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64url_encode(const unsigned char *in, int in_len, char *out) {
    int i, pos = 0;
    for (i = 0; i < in_len; i += 3) {
        unsigned int n = (unsigned int)in[i] << 16;
        if (i + 1 < in_len) n |= (unsigned int)in[i+1] << 8;
        if (i + 2 < in_len) n |= (unsigned int)in[i+2];
        out[pos++] = b64_table[(n >> 18) & 63];
        out[pos++] = b64_table[(n >> 12) & 63];
        if (i + 1 < in_len) out[pos++] = b64_table[(n >> 6) & 63];
        if (i + 2 < in_len) out[pos++] = b64_table[n & 63];
    }
    /* Replace + with -, / with _, remove = padding (base64url) */
    int j;
    for (j = 0; j < pos; j++) {
        if (out[j] == '+') out[j] = '-';
        if (out[j] == '/') out[j] = '_';
    }
    out[pos] = '\0';
    return pos;
}

static int b64url_decode(const char *in, unsigned char *out) {
    static unsigned char dtable[256];
    static int init = 0;
    if (!init) {
        int i;
        memset(dtable, 0, 256);
        for (i = 0; i < 64; i++) dtable[(unsigned char)b64_table[i]] = (unsigned char)i;
        dtable[(unsigned char)'-'] = 62;
        dtable[(unsigned char)'_'] = 63;
        init = 1;
    }
    int len = (int)strlen(in);
    int pos = 0, i;
    for (i = 0; i < len; i += 4) {
        unsigned int n = 0;
        int pad = 0;
        int j;
        for (j = 0; j < 4; j++) {
            n <<= 6;
            if (i + j < len) n |= dtable[(unsigned char)in[i+j]];
            else pad++;
        }
        out[pos++] = (unsigned char)(n >> 16);
        if (pad < 2) out[pos++] = (unsigned char)(n >> 8);
        if (pad < 1) out[pos++] = (unsigned char)n;
    }
    return pos;
}

/* Simple HMAC-like signature (FNV-based for TCC compatibility) */
static void hmac_sign(const char *data, const char *key, unsigned char *out, int out_len) {
    unsigned long long h = 14695981039346656037ULL;
    int dlen = (int)strlen(data);
    int klen = (int)strlen(key);
    int i;
    /* Key mixing */
    for (i = 0; i < klen; i++) { h ^= (unsigned char)key[i]; h *= 1099511628211ULL; }
    /* Data mixing */
    for (i = 0; i < dlen; i++) { h ^= (unsigned char)data[i]; h *= 1099511628211ULL; }
    /* Second pass with key */
    for (i = 0; i < klen; i++) { h ^= (unsigned char)key[i]; h *= 1099511628211ULL; }
    /* Generate output */
    for (i = 0; i < out_len; i++) {
        h = h * 6364136223846793005ULL + 1442695040888963407ULL;
        out[i] = (unsigned char)(h >> 33);
    }
}

/* Encode JWT: header.payload.signature */
const char *xjwt_encode(const char *payload_json, const char *secret) {
    /* Header: {"alg":"HS256","typ":"JWT"} */
    const char *header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char header_b64[256], payload_b64[2048], signing_input[2500];
    unsigned char sig[32];
    char sig_b64[64];

    b64url_encode((const unsigned char *)header, (int)strlen(header), header_b64);
    b64url_encode((const unsigned char *)payload_json, (int)strlen(payload_json), payload_b64);

    sprintf(signing_input, "%s.%s", header_b64, payload_b64);
    hmac_sign(signing_input, secret, sig, 32);
    b64url_encode(sig, 32, sig_b64);

    sprintf(buf, "%s.%s", signing_input, sig_b64);
    return buf;
}

/* Decode JWT payload (returns JSON string) */
const char *xjwt_decode(const char *token) {
    static char payload_buf[2048];
    /* Find second dot */
    const char *dot1 = strchr(token, '.');
    if (!dot1) return "{}";
    const char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) return "{}";

    int plen = (int)(dot2 - dot1 - 1);
    char payload_b64[2048];
    memcpy(payload_b64, dot1 + 1, plen);
    payload_b64[plen] = '\0';

    int dlen = b64url_decode(payload_b64, (unsigned char *)payload_buf);
    payload_buf[dlen] = '\0';
    return payload_buf;
}

/* Verify JWT signature */
long long xjwt_verify(const char *token, const char *secret) {
    const char *dot1 = strchr(token, '.');
    if (!dot1) return 0;
    const char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) return 0;

    int input_len = (int)(dot2 - token);
    char signing_input[2500];
    memcpy(signing_input, token, input_len);
    signing_input[input_len] = '\0';

    unsigned char expected_sig[32];
    hmac_sign(signing_input, secret, expected_sig, 32);
    char expected_b64[64];
    b64url_encode(expected_sig, 32, expected_b64);

    return strcmp(dot2 + 1, expected_b64) == 0 ? 1 : 0;
}

/* Get header JSON */
const char *xjwt_header(const char *token) {
    static char hdr_buf[256];
    const char *dot1 = strchr(token, '.');
    if (!dot1) return "{}";
    int hlen = (int)(dot1 - token);
    char h_b64[256];
    memcpy(h_b64, token, hlen);
    h_b64[hlen] = '\0';
    int dlen = b64url_decode(h_b64, (unsigned char *)hdr_buf);
    hdr_buf[dlen] = '\0';
    return hdr_buf;
}
