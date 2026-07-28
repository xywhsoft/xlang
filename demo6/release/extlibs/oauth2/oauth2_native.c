/* oauth2_native.c - OAuth2 flow helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];

/* URL-encode a string */
static int url_encode(const char *in, char *out) {
    int pos = 0;
    while (*in) {
        if ((*in >= 'A' && *in <= 'Z') || (*in >= 'a' && *in <= 'z') ||
            (*in >= '0' && *in <= '9') || *in == '-' || *in == '_' || *in == '.' || *in == '~') {
            out[pos++] = *in;
        } else if (*in == ' ') {
            out[pos++] = '+';
        } else {
            sprintf(out + pos, "%%%02X", (unsigned char)*in);
            pos += 3;
        }
        in++;
    }
    out[pos] = '\0';
    return pos;
}

/* Build authorization URL */
const char *xoauth2_auth_url(const char *base_url, const char *client_id, const char *redirect_uri, const char *scope, const char *state) {
    char enc_redirect[1024], enc_scope[1024], enc_state[512], enc_client[512];
    url_encode(client_id, enc_client);
    url_encode(redirect_uri, enc_redirect);
    url_encode(scope, enc_scope);
    url_encode(state, enc_state);
    sprintf(buf, "%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s&state=%s",
            base_url, enc_client, enc_redirect, enc_scope, enc_state);
    return buf;
}

/* Build token request body (form-encoded) */
const char *xoauth2_token_body(const char *code, const char *client_id, const char *client_secret, const char *redirect_uri) {
    char enc_code[1024], enc_client[512], enc_secret[512], enc_redirect[1024];
    url_encode(code, enc_code);
    url_encode(client_id, enc_client);
    url_encode(client_secret, enc_secret);
    url_encode(redirect_uri, enc_redirect);
    sprintf(buf, "grant_type=authorization_code&code=%s&client_id=%s&client_secret=%s&redirect_uri=%s",
            enc_code, enc_client, enc_secret, enc_redirect);
    return buf;
}

/* Build refresh token request body */
const char *xoauth2_refresh_body(const char *refresh_token, const char *client_id, const char *client_secret) {
    char enc_token[1024], enc_client[512], enc_secret[512];
    url_encode(refresh_token, enc_token);
    url_encode(client_id, enc_client);
    url_encode(client_secret, enc_secret);
    sprintf(buf, "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s",
            enc_token, enc_client, enc_secret);
    return buf;
}

/* Build client credentials request body */
const char *xoauth2_client_creds_body(const char *client_id, const char *client_secret, const char *scope) {
    char enc_client[512], enc_secret[512], enc_scope[1024];
    url_encode(client_id, enc_client);
    url_encode(client_secret, enc_secret);
    url_encode(scope, enc_scope);
    sprintf(buf, "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=%s",
            enc_client, enc_secret, enc_scope);
    return buf;
}

/* Generate PKCE code verifier (deterministic from seed) */
const char *xoauth2_pkce_verifier(long long seed) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    unsigned long long h = (unsigned long long)seed * 6364136223846793005ULL + 1442695040888963407ULL;
    int i;
    for (i = 0; i < 43; i++) {
        h = h * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = chars[(h >> 33) % 66];
    }
    buf[43] = '\0';
    return buf;
}

/* Compute PKCE S256 challenge (simplified FNV-based hash, base64url-like) */
const char *xoauth2_pkce_challenge(const char *verifier) {
    unsigned long long h = 14695981039346656037ULL;
    int len = (int)strlen(verifier);
    int i;
    for (i = 0; i < len; i++) { h ^= (unsigned char)verifier[i]; h *= 1099511628211ULL; }
    /* Generate 32 bytes of hash output, encode as base64url-like */
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    unsigned char hash[32];
    for (i = 0; i < 32; i++) { h = h * 6364136223846793005ULL + 1442695040888963407ULL; hash[i] = (unsigned char)(h >> 33); }
    int pos = 0;
    for (i = 0; i < 32; i += 3) {
        unsigned int n = (unsigned int)hash[i] << 16;
        if (i+1 < 32) n |= (unsigned int)hash[i+1] << 8;
        if (i+2 < 32) n |= (unsigned int)hash[i+2];
        buf[pos++] = b64[(n >> 18) & 63];
        buf[pos++] = b64[(n >> 12) & 63];
        if (i+1 < 32) buf[pos++] = b64[(n >> 6) & 63];
        if (i+2 < 32) buf[pos++] = b64[n & 63];
    }
    buf[pos] = '\0';
    return buf;
}

/* Validate a state parameter (non-empty, reasonable length) */
long long xoauth2_valid_state(const char *state) {
    int len = (int)strlen(state);
    return (len >= 8 && len <= 128) ? 1 : 0;
}

/* Get grant type name */
const char *xoauth2_grant_name(const char *grant_type) {
    if (strcmp(grant_type, "authorization_code") == 0) return "Authorization Code";
    if (strcmp(grant_type, "client_credentials") == 0) return "Client Credentials";
    if (strcmp(grant_type, "refresh_token") == 0) return "Refresh Token";
    if (strcmp(grant_type, "implicit") == 0) return "Implicit";
    return "Unknown";
}
