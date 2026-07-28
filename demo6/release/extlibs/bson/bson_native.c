/* bson_native.c - BSON (Binary JSON) encoder/decoder */
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

static void put_i32(unsigned char *buf, int pos, int val) {
    buf[pos]   = (unsigned char)(val & 0xFF);
    buf[pos+1] = (unsigned char)((val >> 8) & 0xFF);
    buf[pos+2] = (unsigned char)((val >> 16) & 0xFF);
    buf[pos+3] = (unsigned char)((val >> 24) & 0xFF);
}

static int get_i32(const unsigned char *buf, int pos) {
    return (int)buf[pos] | ((int)buf[pos+1]<<8) | ((int)buf[pos+2]<<16) | ((int)buf[pos+3]<<24);
}

/* Wrap elements hex into a BSON document */
const char *xbson_encode_doc(const char *elements_hex) {
    unsigned char elems[8192];
    int elen = from_hex(elements_hex, elems);
    int doc_len = 4 + elen + 1; /* int32 len + elements + 0x00 */
    unsigned char doc[8300];
    put_i32(doc, 0, doc_len);
    memcpy(doc + 4, elems, elen);
    doc[4 + elen] = 0x00;
    to_hex(doc, doc_len, hex_buf);
    return hex_buf;
}

/* Encode string element: type(0x02) + key + 0x00 + int32 strlen + string + 0x00 */
const char *xbson_encode_str(const char *key, const char *value) {
    unsigned char buf[4096]; int pos = 0;
    int klen = (int)strlen(key), vlen = (int)strlen(value);
    buf[pos++] = 0x02;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    put_i32(buf, pos, vlen + 1); pos += 4;
    memcpy(buf + pos, value, vlen); pos += vlen;
    buf[pos++] = 0x00;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Encode int32 element: type(0x10) + key + 0x00 + int32 */
const char *xbson_encode_int32(const char *key, long long value) {
    unsigned char buf[256]; int pos = 0;
    int klen = (int)strlen(key);
    buf[pos++] = 0x10;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    put_i32(buf, pos, (int)value); pos += 4;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Encode int64 element: type(0x12) + key + 0x00 + int64 */
const char *xbson_encode_int64(const char *key, long long value) {
    unsigned char buf[256]; int pos = 0;
    int klen = (int)strlen(key);
    buf[pos++] = 0x12;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    int i; for (i = 0; i < 8; i++) buf[pos+i] = (unsigned char)(value >> (i*8));
    pos += 8;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Encode double element: type(0x01) + key + 0x00 + double(8 bytes LE) */
const char *xbson_encode_double(const char *key, double value) {
    unsigned char buf[256]; int pos = 0;
    int klen = (int)strlen(key);
    buf[pos++] = 0x01;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    unsigned long long bits; memcpy(&bits, &value, 8);
    int i; for (i = 0; i < 8; i++) buf[pos+i] = (unsigned char)(bits >> (i*8));
    pos += 8;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Encode boolean element: type(0x08) + key + 0x00 + byte */
const char *xbson_encode_bool(const char *key, long long value) {
    unsigned char buf[256]; int pos = 0;
    int klen = (int)strlen(key);
    buf[pos++] = 0x08;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    buf[pos++] = value ? 0x01 : 0x00;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Encode null element: type(0x0A) + key + 0x00 */
const char *xbson_encode_null(const char *key) {
    unsigned char buf[256]; int pos = 0;
    int klen = (int)strlen(key);
    buf[pos++] = 0x0A;
    memcpy(buf + pos, key, klen); pos += klen;
    buf[pos++] = 0x00;
    to_hex(buf, pos, hex_buf);
    return hex_buf;
}

/* Get document length from hex */
long long xbson_doc_len(const char *bson_hex) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    return (long long)get_i32(buf, 0);
}

/* Get element type at offset (byte offset into the binary) */
long long xbson_elem_type(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    return (long long)buf[(int)offset];
}

/* Get element key at offset */
const char *xbson_elem_key(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1; /* skip type byte */
    int klen = 0;
    while (buf[pos + klen] != 0) klen++;
    memcpy(str_buf, buf + pos, klen);
    str_buf[klen] = '\0';
    return str_buf;
}

/* Decode string value at element offset */
const char *xbson_decode_str(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1;
    while (buf[pos] != 0) pos++; /* skip key */
    pos++; /* skip null */
    int slen = get_i32(buf, pos) - 1; pos += 4;
    memcpy(str_buf, buf + pos, slen);
    str_buf[slen] = '\0';
    return str_buf;
}

/* Decode int32 value at element offset */
long long xbson_decode_int32(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1;
    while (buf[pos] != 0) pos++;
    pos++;
    return (long long)get_i32(buf, pos);
}

/* Decode int64 value at element offset */
long long xbson_decode_int64(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1;
    while (buf[pos] != 0) pos++;
    pos++;
    long long val = 0;
    int i; for (i = 0; i < 8; i++) val |= (long long)buf[pos+i] << (i*8);
    return val;
}

/* Decode double value at element offset */
double xbson_decode_double(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1;
    while (buf[pos] != 0) pos++;
    pos++;
    unsigned long long bits = 0;
    int i; for (i = 0; i < 8; i++) bits |= (unsigned long long)buf[pos+i] << (i*8);
    double val; memcpy(&val, &bits, 8);
    return val;
}

/* Decode bool value at element offset */
long long xbson_decode_bool(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; from_hex(bson_hex, buf);
    int pos = (int)offset + 1;
    while (buf[pos] != 0) pos++;
    pos++;
    return buf[pos] ? 1 : 0;
}

/* Get next element offset (returns -1 at end) */
long long xbson_next_elem(const char *bson_hex, long long offset) {
    unsigned char buf[8192]; int blen = from_hex(bson_hex, buf);
    int pos = (int)offset;
    if (pos >= blen - 1) return -1;
    unsigned char type = buf[pos]; pos++;
    while (pos < blen && buf[pos] != 0) pos++; /* skip key */
    pos++; /* skip null */
    switch (type) {
        case 0x01: pos += 8; break; /* double */
        case 0x02: { int sl = get_i32(buf, pos); pos += 4 + sl; break; } /* string */
        case 0x03: case 0x04: { int dl = get_i32(buf, pos); pos += dl; break; } /* doc/array */
        case 0x05: { int bl = get_i32(buf, pos); pos += 4 + 1 + bl; break; } /* binary */
        case 0x08: pos += 1; break; /* bool */
        case 0x09: pos += 8; break; /* datetime */
        case 0x0A: break; /* null */
        case 0x10: pos += 4; break; /* int32 */
        case 0x12: pos += 8; break; /* int64 */
        default: return -1;
    }
    if (pos >= blen - 1) return -1; /* past terminator */
    if (buf[pos] == 0x00) return -1; /* document terminator */
    return (long long)pos;
}

/* Concat hex */
const char *xbson_concat(const char *a, const char *b) {
    static char cat[16384];
    int la = (int)strlen(a);
    memcpy(cat, a, la); strcpy(cat + la, b);
    return cat;
}
