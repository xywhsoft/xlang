/* passkey_native.c - WebAuthn/FIDO2 passkey helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Attestation format name */
const char* xpasskey_fmt_name(const char *fmt) {
    if (strcmp(fmt, "none") == 0) return "None";
    if (strcmp(fmt, "packed") == 0) return "Packed";
    if (strcmp(fmt, "fido-u2f") == 0) return "FIDO U2F";
    if (strcmp(fmt, "android-key") == 0) return "Android Key";
    if (strcmp(fmt, "android-safetynet") == 0) return "Android SafetyNet";
    if (strcmp(fmt, "apple") == 0) return "Apple";
    if (strcmp(fmt, "tpm") == 0) return "TPM";
    return "Unknown";
}

/* COSE algorithm name from identifier */
const char* xpasskey_cose_algo(long long algo_id) {
    switch (algo_id) {
        case -7: return "ES256";
        case -8: return "EdDSA";
        case -35: return "ES384";
        case -36: return "ES512";
        case -257: return "RS256";
        case -258: return "RS384";
        case -259: return "RS512";
        case -65535: return "ECDH-ES";
        default: return "Unknown";
    }
}

/* COSE key type name */
const char* xpasskey_cose_kty(long long kty) {
    switch (kty) {
        case 1: return "OKP";
        case 2: return "EC2";
        case 3: return "RSA";
        default: return "Unknown";
    }
}

/* COSE curve name */
const char* xpasskey_cose_curve(long long crv) {
    switch (crv) {
        case 1: return "P-256";
        case 2: return "P-384";
        case 3: return "P-521";
        case 4: return "X25519";
        case 5: return "X448";
        case 6: return "Ed25519";
        case 7: return "Ed448";
        default: return "Unknown";
    }
}

/* Authenticator transport name */
const char* xpasskey_transport_name(long long transport) {
    switch (transport) {
        case 0: return "usb";
        case 1: return "nfc";
        case 2: return "ble";
        case 3: return "internal";
        case 4: return "hybrid";
        default: return "unknown";
    }
}

/* User verification requirement name */
const char* xpasskey_uv_name(long long uv) {
    switch (uv) {
        case 0: return "discouraged";
        case 1: return "preferred";
        case 2: return "required";
        default: return "unknown";
    }
}

/* Resident key requirement name */
const char* xpasskey_rk_name(long long rk) {
    switch (rk) {
        case 0: return "discouraged";
        case 1: return "preferred";
        case 2: return "required";
        default: return "unknown";
    }
}

/* Attestation conveyance preference name */
const char* xpasskey_attestation_name(long long att) {
    switch (att) {
        case 0: return "none";
        case 1: return "indirect";
        case 2: return "direct";
        case 3: return "enterprise";
        default: return "unknown";
    }
}

/* Generate a simple challenge hex string from seed (deterministic for testing) */
const char* xpasskey_gen_challenge(long long seed, int len) {
    if (len > 64) len = 64;
    if (len < 1) len = 32;
    unsigned long long s = (unsigned long long)seed;
    int pos = 0;
    for (int i = 0; i < len; i++) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        pos += sprintf(str_buf + pos, "%02x", (int)((s >> 33) & 0xFF));
    }
    return str_buf;
}

/* Validate RP ID: must be a valid domain (simplified check) */
long long xpasskey_valid_rpid(const char *rpid) {
    int len = (int)strlen(rpid);
    if (len == 0 || len > 253) return 0;
    if (rpid[0] == '.' || rpid[len-1] == '.') return 0;
    for (int i = 0; i < len; i++) {
        char c = rpid[i];
        if (c != '.' && c != '-' && !(c >= 'a' && c <= 'z') &&
            !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) return 0;
    }
    /* Must have at least one dot for a domain (or be localhost) */
    if (strcmp(rpid, "localhost") == 0) return 1;
    if (strchr(rpid, '.') == NULL) return 0;
    return 1;
}

/* Check if origin matches RP ID (simplified) */
long long xpasskey_origin_matches(const char *origin, const char *rpid) {
    /* origin like "https://example.com", rpid like "example.com" */
    const char *host = origin;
    if (strncmp(origin, "https://", 8) == 0) host = origin + 8;
    else if (strncmp(origin, "http://", 7) == 0) host = origin + 7;
    else return 0;
    /* Strip port if present */
    char hbuf[256];
    int i = 0;
    while (host[i] && host[i] != ':' && host[i] != '/' && i < 255) {
        hbuf[i] = host[i]; i++;
    }
    hbuf[i] = '\0';
    /* Exact match or subdomain */
    if (strcmp(hbuf, rpid) == 0) return 1;
    int hlen = (int)strlen(hbuf), rlen = (int)strlen(rpid);
    if (hlen > rlen && strcmp(hbuf + hlen - rlen, rpid) == 0 && hbuf[hlen - rlen - 1] == '.') return 1;
    return 0;
}

/* Authenticator data flags decode */
const char* xpasskey_flags_str(long long flags) {
    static char fbuf[16];
    int pos = 0;
    if (flags & 0x01) fbuf[pos++] = 'U'; /* User Present */
    if (flags & 0x04) fbuf[pos++] = 'V'; /* User Verified */
    if (flags & 0x40) fbuf[pos++] = 'A'; /* Attested Credential Data */
    if (flags & 0x80) fbuf[pos++] = 'E'; /* Extension Data */
    fbuf[pos] = '\0';
    return fbuf;
}
