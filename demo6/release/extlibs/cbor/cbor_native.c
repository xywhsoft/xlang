/* cbor_native.c - CBOR (RFC 8949) encoder/decoder */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[16384];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i;
    for (i = 0; i < len; i++) sprintf(out + i*2, "%02x", data[i]);
    out[len*2] = '\0';
}

static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex) / 2, i;
    for (i = 0; i < len; i++) { unsigned int v; sscanf(hex+i*2, "%02x", &v); out[i] = (unsigned char)v; }
    return len;
}

/* Encode CBOR head (major type + argument) */
static int enc_head(unsigned char *out, int mt, unsigned long long val) {
    int pos = 0;
    unsigned char mb = (unsigned char)(mt << 5);
    if (val < 24) { out[pos++] = mb | (unsigned char)val; }
    else if (val < 256) { out[pos++] = mb | 24; out[pos++] = (unsigned char)val; }
    else if (val < 65536) { out[pos++] = mb | 25; out[pos++] = (unsigned char)(val>>8); out[pos++] = (unsigned char)val; }
    else if (val < 4294967296ULL) { out[pos++] = mb | 26; out[pos++] = (unsigned char)(val>>24); out[pos++] = (unsigned char)(val>>16); out[pos++] = (unsigned char)(val>>8); out[pos++] = (unsigned char)val; }
    else { out[pos++] = mb | 27; int i; for(i=7;i>=0;i--) out[pos++] = (unsigned char)(val>>(i*8)); }
    return pos;
}

/* Decode CBOR head, returns major type, sets *val and *bytes_consumed */
static int dec_head(const unsigned char *in, int in_len, int *mt, unsigned long long *val, int *consumed) {
    if (in_len < 1) return -1;
    *mt = in[0] >> 5;
    int ai = in[0] & 0x1F;
    int pos = 1;
    if (ai < 24) { *val = ai; }
    else if (ai == 24) { if (in_len < 2) return -1; *val = in[1]; pos = 2; }
    else if (ai == 25) { if (in_len < 3) return -1; *val = ((unsigned long long)in[1]<<8)|in[2]; pos = 3; }
    else if (ai == 26) { if (in_len < 5) return -1; *val = ((unsigned long long)in[1]<<24)|((unsigned long long)in[2]<<16)|((unsigned long long)in[3]<<8)|in[4]; pos = 5; }
    else if (ai == 27) { if (in_len < 9) return -1; *val = 0; int i; for(i=0;i<8;i++) *val = (*val<<8)|in[1+i]; pos = 9; }
    else { *val = 0; }
    *consumed = pos;
    return 0;
}

/* Encode unsigned int */
const char *xcbor_encode_uint(long long value) {
    unsigned char buf[16]; int len = enc_head(buf, 0, (unsigned long long)value);
    to_hex(buf, len, hex_buf); return hex_buf;
}

/* Encode negative int (takes the actual negative value) */
const char *xcbor_encode_int(long long value) {
    unsigned char buf[16]; int len;
    if (value >= 0) len = enc_head(buf, 0, (unsigned long long)value);
    else len = enc_head(buf, 1, (unsigned long long)(-1 - value));
    to_hex(buf, len, hex_buf); return hex_buf;
}

/* Decode integer */
long long xcbor_decode_int(const char *cbor_hex) {
    unsigned char buf[64]; int blen = from_hex(cbor_hex, buf);
    int mt, consumed; unsigned long long val;
    dec_head(buf, blen, &mt, &val, &consumed);
    if (mt == 0) return (long long)val;
    if (mt == 1) return -1 - (long long)val;
    return 0;
}

/* Encode text string */
const char *xcbor_encode_str(const char *s) {
    unsigned char buf[4096]; int slen = (int)strlen(s);
    int hlen = enc_head(buf, 3, (unsigned long long)slen);
    memcpy(buf + hlen, s, slen);
    to_hex(buf, hlen + slen, hex_buf); return hex_buf;
}

/* Decode text string */
const char *xcbor_decode_str(const char *cbor_hex) {
    unsigned char buf[4096]; int blen = from_hex(cbor_hex, buf);
    int mt, consumed; unsigned long long val;
    dec_head(buf, blen, &mt, &val, &consumed);
    int slen = (int)val;
    memcpy(str_buf, buf + consumed, slen);
    str_buf[slen] = '\0';
    return str_buf;
}

/* Encode byte string from hex */
const char *xcbor_encode_bytes(const char *data_hex) {
    unsigned char data[4096]; int dlen = from_hex(data_hex, data);
    unsigned char buf[4200]; int hlen = enc_head(buf, 2, (unsigned long long)dlen);
    memcpy(buf + hlen, data, dlen);
    to_hex(buf, hlen + dlen, hex_buf); return hex_buf;
}

/* Decode byte string to hex */
const char *xcbor_decode_bytes(const char *cbor_hex) {
    unsigned char buf[4096]; int blen = from_hex(cbor_hex, buf);
    int mt, consumed; unsigned long long val;
    dec_head(buf, blen, &mt, &val, &consumed);
    to_hex(buf + consumed, (int)val, hex_buf); return hex_buf;
}

/* Encode array header (definite length) */
const char *xcbor_encode_array_hdr(long long count) {
    unsigned char buf[16]; int len = enc_head(buf, 4, (unsigned long long)count);
    to_hex(buf, len, hex_buf); return hex_buf;
}

/* Encode map header (definite length, pairs count) */
const char *xcbor_encode_map_hdr(long long pairs) {
    unsigned char buf[16]; int len = enc_head(buf, 5, (unsigned long long)pairs);
    to_hex(buf, len, hex_buf); return hex_buf;
}

/* Encode boolean */
const char *xcbor_encode_bool(long long val) {
    unsigned char b = val ? 0xF5 : 0xF4;
    sprintf(hex_buf, "%02x", b); return hex_buf;
}

/* Encode null */
const char *xcbor_encode_null(void) { return "f6"; }

/* Encode float64 */
const char *xcbor_encode_float(double val) {
    unsigned char buf[9]; buf[0] = 0xFB;
    unsigned long long bits; memcpy(&bits, &val, 8);
    int i; for (i = 7; i >= 0; i--) buf[8-i] = (unsigned char)(bits >> (i*8));
    to_hex(buf, 9, hex_buf); return hex_buf;
}

/* Decode float64 */
double xcbor_decode_float(const char *cbor_hex) {
    unsigned char buf[16]; from_hex(cbor_hex, buf);
    unsigned long long bits = 0;
    int i; for (i = 0; i < 8; i++) bits = (bits << 8) | buf[1+i];
    double val; memcpy(&val, &bits, 8);
    return val;
}

/* Get major type (0-7) */
long long xcbor_get_type(const char *cbor_hex) {
    unsigned char buf[16]; from_hex(cbor_hex, buf);
    return (long long)(buf[0] >> 5);
}

/* Get type name */
const char *xcbor_type_name(const char *cbor_hex) {
    unsigned char buf[16]; from_hex(cbor_hex, buf);
    int mt = buf[0] >> 5;
    switch(mt) {
        case 0: return "uint";
        case 1: return "nint";
        case 2: return "bytes";
        case 3: return "text";
        case 4: return "array";
        case 5: return "map";
        case 6: return "tag";
        case 7:
            if (buf[0] == 0xF4 || buf[0] == 0xF5) return "bool";
            if (buf[0] == 0xF6) return "null";
            if (buf[0] == 0xFB) return "float";
            return "simple";
        default: return "unknown";
    }
}

/* Get total byte length of first CBOR item */
long long xcbor_item_len(const char *cbor_hex) {
    unsigned char buf[8192]; int blen = from_hex(cbor_hex, buf);
    int mt, consumed; unsigned long long val;
    dec_head(buf, blen, &mt, &val, &consumed);
    long long total = consumed;
    if (mt == 2 || mt == 3) total += (long long)val;
    /* For arrays/maps, we'd need recursive parsing - simplified: just header */
    return total;
}

/* Concatenate two hex strings */
const char *xcbor_concat(const char *a, const char *b) {
    static char cat[16384];
    int la = (int)strlen(a);
    memcpy(cat, a, la); strcpy(cat + la, b);
    return cat;
}

/* Decode boolean */
long long xcbor_decode_bool(const char *cbor_hex) {
    unsigned char buf[4]; from_hex(cbor_hex, buf);
    return buf[0] == 0xF5 ? 1 : 0;
}
