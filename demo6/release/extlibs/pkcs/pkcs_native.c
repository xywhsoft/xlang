/* pkcs_native.c - ASN.1 DER encoding/decoding subset */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i;
    for (i = 0; i < len; i++)
        sprintf(out + i * 2, "%02x", data[i]);
    out[len * 2] = '\0';
}

static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex) / 2;
    int i;
    for (i = 0; i < len; i++) {
        unsigned int v;
        sscanf(hex + i * 2, "%02x", &v);
        out[i] = (unsigned char)v;
    }
    return len;
}

/* DER tag constants */
#define TAG_INTEGER      0x02
#define TAG_BIT_STRING   0x03
#define TAG_OCTET_STRING 0x04
#define TAG_NULL         0x05
#define TAG_OID          0x06
#define TAG_SEQUENCE     0x30
#define TAG_SET          0x31

/* Encode DER length */
static int der_encode_len(int len, unsigned char *out) {
    if (len < 0x80) {
        out[0] = (unsigned char)len;
        return 1;
    } else if (len < 0x100) {
        out[0] = 0x81;
        out[1] = (unsigned char)len;
        return 2;
    } else {
        out[0] = 0x82;
        out[1] = (unsigned char)(len >> 8);
        out[2] = (unsigned char)(len & 0xFF);
        return 3;
    }
}

/* Encode a TLV and return hex */
static const char *make_tlv(int tag, const unsigned char *content, int content_len) {
    static unsigned char der[4096];
    int pos = 0;
    der[pos++] = (unsigned char)tag;
    pos += der_encode_len(content_len, der + pos);
    memcpy(der + pos, content, content_len);
    pos += content_len;
    to_hex(der, pos, hex_buf);
    return hex_buf;
}

/* Get tag of a DER hex string */
long long xpkcs_get_tag(const char *der_hex) {
    unsigned char der[4096];
    from_hex(der_hex, der);
    return (long long)der[0];
}

/* Get content length of a DER hex string */
long long xpkcs_get_length(const char *der_hex) {
    unsigned char der[4096];
    from_hex(der_hex, der);
    int pos = 1;
    if (der[pos] < 0x80) return (long long)der[pos];
    int num_bytes = der[pos] & 0x7F;
    pos++;
    long long len = 0;
    int i;
    for (i = 0; i < num_bytes; i++)
        len = (len << 8) | der[pos + i];
    return len;
}

/* Get content bytes as hex (skip tag + length) */
const char *xpkcs_get_content(const char *der_hex) {
    static unsigned char content[4096];
    unsigned char der[4096];
    int dlen = from_hex(der_hex, der);
    int pos = 1;
    int clen;
    if (der[pos] < 0x80) {
        clen = der[pos];
        pos += 1;
    } else {
        int num_bytes = der[pos] & 0x7F;
        pos++;
        clen = 0;
        int i;
        for (i = 0; i < num_bytes; i++)
            clen = (clen << 8) | der[pos + i];
        pos += num_bytes;
    }
    memcpy(content, der + pos, clen);
    to_hex(content, clen, hex_buf);
    return hex_buf;
}

/* Encode integer value to DER hex */
const char *xpkcs_encode_int(long long value) {
    unsigned char content[9];
    int clen = 0;
    unsigned long long v;
    if (value == 0) {
        content[0] = 0;
        clen = 1;
    } else if (value > 0) {
        v = (unsigned long long)value;
        unsigned char tmp[8];
        int tlen = 0;
        while (v > 0) { tmp[tlen++] = (unsigned char)(v & 0xFF); v >>= 8; }
        /* Reverse */
        int i;
        for (i = tlen - 1; i >= 0; i--) content[clen++] = tmp[i];
        /* If high bit set, prepend 0x00 */
        if (content[0] & 0x80) {
            memmove(content + 1, content, clen);
            content[0] = 0;
            clen++;
        }
    } else {
        /* Negative: two's complement */
        v = (unsigned long long)value;
        unsigned char tmp[8];
        int tlen = 0;
        int i;
        for (i = 0; i < 8; i++) tmp[i] = (unsigned char)((v >> (i * 8)) & 0xFF);
        /* Find minimal representation */
        tlen = 8;
        while (tlen > 1 && tmp[tlen-1] == 0xFF && (tmp[tlen-2] & 0x80)) tlen--;
        for (i = tlen - 1; i >= 0; i--) content[clen++] = tmp[i];
    }
    return make_tlv(TAG_INTEGER, content, clen);
}

/* Decode integer from DER hex */
long long xpkcs_decode_int(const char *der_hex) {
    unsigned char der[4096];
    from_hex(der_hex, der);
    int pos = 1;
    int clen;
    if (der[pos] < 0x80) { clen = der[pos]; pos++; }
    else {
        int nb = der[pos] & 0x7F; pos++;
        clen = 0;
        int i;
        for (i = 0; i < nb; i++) clen = (clen << 8) | der[pos+i];
        pos += nb;
    }
    /* Parse integer value */
    long long val = (der[pos] & 0x80) ? -1 : 0;
    int i;
    for (i = 0; i < clen; i++)
        val = (val << 8) | der[pos + i];
    return val;
}

/* Encode OID from dotted string (e.g. "1.2.840.113549.1.1.1") */
const char *xpkcs_encode_oid(const char *dotted) {
    unsigned char content[128];
    int clen = 0;
    unsigned long long components[32];
    int ncomp = 0;

    /* Parse dotted notation */
    const char *p = dotted;
    while (*p) {
        unsigned long long v = 0;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
        components[ncomp++] = v;
        if (*p == '.') p++;
    }
    if (ncomp < 2) return "";

    /* First two components encoded as 40*X + Y */
    unsigned long long first = components[0] * 40 + components[1];
    int i;
    /* Encode first combined value */
    {
        unsigned long long v = first;
        unsigned char tmp[8];
        int tlen = 0;
        tmp[tlen++] = (unsigned char)(v & 0x7F);
        v >>= 7;
        while (v > 0) { tmp[tlen++] = (unsigned char)((v & 0x7F) | 0x80); v >>= 7; }
        for (i = tlen - 1; i >= 0; i--) content[clen++] = tmp[i];
    }
    /* Encode remaining components */
    int c;
    for (c = 2; c < ncomp; c++) {
        unsigned long long v = components[c];
        unsigned char tmp[8];
        int tlen = 0;
        tmp[tlen++] = (unsigned char)(v & 0x7F);
        v >>= 7;
        while (v > 0) { tmp[tlen++] = (unsigned char)((v & 0x7F) | 0x80); v >>= 7; }
        for (i = tlen - 1; i >= 0; i--) content[clen++] = tmp[i];
    }
    return make_tlv(TAG_OID, content, clen);
}

/* Decode OID to dotted string */
const char *xpkcs_decode_oid(const char *der_hex) {
    static char oid_str[256];
    unsigned char der[4096];
    from_hex(der_hex, der);
    int pos = 1;
    int clen;
    if (der[pos] < 0x80) { clen = der[pos]; pos++; }
    else {
        int nb = der[pos] & 0x7F; pos++;
        clen = 0;
        int i;
        for (i = 0; i < nb; i++) clen = (clen << 8) | der[pos+i];
        pos += nb;
    }
    /* Decode first byte into two components */
    unsigned int first_byte = der[pos];
    int opos = sprintf(oid_str, "%d.%d", first_byte / 40, first_byte % 40);
    pos++;
    clen--;
    /* Decode remaining */
    while (clen > 0) {
        unsigned long long v = 0;
        while (clen > 0) {
            v = (v << 7) | (der[pos] & 0x7F);
            int more = der[pos] & 0x80;
            pos++; clen--;
            if (!more) break;
        }
        opos += sprintf(oid_str + opos, ".%llu", v);
    }
    return oid_str;
}

/* Wrap content hex in a SEQUENCE TLV */
const char *xpkcs_wrap_sequence(const char *content_hex) {
    unsigned char content[4096];
    int clen = from_hex(content_hex, content);
    return make_tlv(TAG_SEQUENCE, content, clen);
}

/* Wrap content hex in a SET TLV */
const char *xpkcs_wrap_set(const char *content_hex) {
    unsigned char content[4096];
    int clen = from_hex(content_hex, content);
    return make_tlv(TAG_SET, content, clen);
}

/* Encode OCTET STRING from hex content */
const char *xpkcs_encode_octet(const char *data_hex) {
    unsigned char content[4096];
    int clen = from_hex(data_hex, content);
    return make_tlv(TAG_OCTET_STRING, content, clen);
}

/* Encode NULL */
const char *xpkcs_encode_null(void) {
    return make_tlv(TAG_NULL, (const unsigned char *)"", 0);
}

/* Encode BIT STRING from hex content (prepends 0x00 unused-bits byte) */
const char *xpkcs_encode_bitstring(const char *data_hex) {
    unsigned char content[4096];
    int dlen = from_hex(data_hex, content);
    unsigned char wrapped[4097];
    wrapped[0] = 0x00; /* no unused bits */
    memcpy(wrapped + 1, content, dlen);
    return make_tlv(TAG_BIT_STRING, wrapped, dlen + 1);
}

/* Concatenate two hex strings (for building SEQUENCE contents) */
const char *xpkcs_concat(const char *hex1, const char *hex2) {
    static char cat_buf[8192];
    int l1 = (int)strlen(hex1);
    memcpy(cat_buf, hex1, l1);
    strcpy(cat_buf + l1, hex2);
    return cat_buf;
}

/* Get total DER byte length from hex */
long long xpkcs_der_len(const char *der_hex) {
    return (long long)(strlen(der_hex) / 2);
}

/* Parse: get next TLV offset (returns byte offset past current TLV) */
long long xpkcs_next_offset(const char *der_hex, long long offset) {
    unsigned char der[4096];
    int dlen = from_hex(der_hex, der);
    int pos = (int)offset;
    if (pos >= dlen) return -1;
    pos++; /* skip tag */
    int clen;
    if (der[pos] < 0x80) { clen = der[pos]; pos++; }
    else {
        int nb = der[pos] & 0x7F; pos++;
        clen = 0;
        int i;
        for (i = 0; i < nb; i++) clen = (clen << 8) | der[pos+i];
        pos += nb;
    }
    return (long long)(pos + clen);
}

/* Get tag name as string */
const char *xpkcs_tag_name(const char *der_hex) {
    static char name_buf[64];
    unsigned char der[4096];
    from_hex(der_hex, der);
    switch (der[0]) {
        case TAG_INTEGER:      strcpy(name_buf, "INTEGER"); break;
        case TAG_BIT_STRING:   strcpy(name_buf, "BIT STRING"); break;
        case TAG_OCTET_STRING: strcpy(name_buf, "OCTET STRING"); break;
        case TAG_NULL:         strcpy(name_buf, "NULL"); break;
        case TAG_OID:          strcpy(name_buf, "OID"); break;
        case TAG_SEQUENCE:     strcpy(name_buf, "SEQUENCE"); break;
        case TAG_SET:          strcpy(name_buf, "SET"); break;
        default: sprintf(name_buf, "TAG(0x%02x)", der[0]); break;
    }
    return name_buf;
}
