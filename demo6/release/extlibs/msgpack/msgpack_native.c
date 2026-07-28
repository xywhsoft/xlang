/* msgpack_native.c - MessagePack encoder/decoder */
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

/* Encode integer */
const char *xmsgpack_encode_int(long long value) {
    unsigned char buf[16]; int pos = 0;
    if (value >= 0 && value <= 127) { buf[pos++] = (unsigned char)value; }
    else if (value < 0 && value >= -32) { buf[pos++] = (unsigned char)(0xe0 | (value + 32)); }
    else if (value >= 0 && value <= 255) { buf[pos++] = 0xcc; buf[pos++] = (unsigned char)value; }
    else if (value >= -128 && value <= 127) { buf[pos++] = 0xd0; buf[pos++] = (unsigned char)(signed char)value; }
    else if (value >= 0 && value <= 65535) { buf[pos++] = 0xcd; buf[pos++] = (unsigned char)(value>>8); buf[pos++] = (unsigned char)value; }
    else if (value >= -32768 && value <= 32767) { buf[pos++] = 0xd1; buf[pos++] = (unsigned char)(value>>8); buf[pos++] = (unsigned char)value; }
    else if (value >= 0 && value <= 4294967295LL) { buf[pos++] = 0xce; int i; for(i=3;i>=0;i--) buf[pos++]=(unsigned char)(value>>(i*8)); }
    else if (value >= -2147483648LL && value <= 2147483647LL) { buf[pos++] = 0xd2; int i; for(i=3;i>=0;i--) buf[pos++]=(unsigned char)(value>>(i*8)); }
    else if (value >= 0) { buf[pos++] = 0xcf; int i; for(i=7;i>=0;i--) buf[pos++]=(unsigned char)(value>>(i*8)); }
    else { buf[pos++] = 0xd3; int i; for(i=7;i>=0;i--) buf[pos++]=(unsigned char)(value>>(i*8)); }
    to_hex(buf, pos, hex_buf); return hex_buf;
}

/* Decode integer */
long long xmsgpack_decode_int(const char *hex) {
    unsigned char buf[16]; from_hex(hex, buf);
    unsigned char c = buf[0];
    if (c <= 0x7f) return c;
    if (c >= 0xe0) return (long long)(signed char)c;
    if (c == 0xcc) return buf[1];
    if (c == 0xd0) return (signed char)buf[1];
    if (c == 0xcd) return (buf[1]<<8)|buf[2];
    if (c == 0xd1) return (short)((buf[1]<<8)|buf[2]);
    if (c == 0xce) return ((long long)buf[1]<<24)|((long long)buf[2]<<16)|((long long)buf[3]<<8)|buf[4];
    if (c == 0xd2) return (int)(((long long)buf[1]<<24)|((long long)buf[2]<<16)|((long long)buf[3]<<8)|buf[4]);
    if (c == 0xcf) { long long v=0; int i; for(i=0;i<8;i++) v=(v<<8)|buf[1+i]; return v; }
    if (c == 0xd3) { long long v=0; int i; for(i=0;i<8;i++) v=(v<<8)|buf[1+i]; return v; }
    return 0;
}

/* Encode string */
const char *xmsgpack_encode_str(const char *s) {
    unsigned char buf[4096]; int slen = (int)strlen(s); int pos = 0;
    if (slen <= 31) { buf[pos++] = (unsigned char)(0xa0 | slen); }
    else if (slen <= 255) { buf[pos++] = 0xd9; buf[pos++] = (unsigned char)slen; }
    else { buf[pos++] = 0xda; buf[pos++] = (unsigned char)(slen>>8); buf[pos++] = (unsigned char)slen; }
    memcpy(buf + pos, s, slen); pos += slen;
    to_hex(buf, pos, hex_buf); return hex_buf;
}

/* Decode string */
const char *xmsgpack_decode_str(const char *hex) {
    unsigned char buf[4096]; from_hex(hex, buf);
    int pos = 0, slen;
    unsigned char c = buf[0];
    if (c >= 0xa0 && c <= 0xbf) { slen = c & 0x1f; pos = 1; }
    else if (c == 0xd9) { slen = buf[1]; pos = 2; }
    else if (c == 0xda) { slen = (buf[1]<<8)|buf[2]; pos = 3; }
    else { slen = 0; pos = 1; }
    memcpy(str_buf, buf + pos, slen);
    str_buf[slen] = '\0';
    return str_buf;
}

/* Encode binary from hex */
const char *xmsgpack_encode_bin(const char *data_hex) {
    unsigned char data[4096]; int dlen = from_hex(data_hex, data);
    unsigned char buf[4200]; int pos = 0;
    if (dlen <= 255) { buf[pos++] = 0xc4; buf[pos++] = (unsigned char)dlen; }
    else { buf[pos++] = 0xc5; buf[pos++] = (unsigned char)(dlen>>8); buf[pos++] = (unsigned char)dlen; }
    memcpy(buf + pos, data, dlen); pos += dlen;
    to_hex(buf, pos, hex_buf); return hex_buf;
}

/* Decode binary to hex */
const char *xmsgpack_decode_bin(const char *hex) {
    unsigned char buf[4096]; from_hex(hex, buf);
    int pos, dlen;
    if (buf[0] == 0xc4) { dlen = buf[1]; pos = 2; }
    else { dlen = (buf[1]<<8)|buf[2]; pos = 3; }
    to_hex(buf + pos, dlen, hex_buf); return hex_buf;
}

/* Encode nil */
const char *xmsgpack_encode_nil(void) { return "c0"; }

/* Encode bool */
const char *xmsgpack_encode_bool(long long val) { return val ? "c3" : "c2"; }

/* Decode bool */
long long xmsgpack_decode_bool(const char *hex) {
    unsigned char buf[4]; from_hex(hex, buf);
    return buf[0] == 0xc3 ? 1 : 0;
}

/* Encode float64 */
const char *xmsgpack_encode_float(double val) {
    unsigned char buf[9]; buf[0] = 0xcb;
    unsigned long long bits; memcpy(&bits, &val, 8);
    int i; for(i=7;i>=0;i--) buf[8-i] = (unsigned char)(bits>>(i*8));
    to_hex(buf, 9, hex_buf); return hex_buf;
}

/* Decode float64 */
double xmsgpack_decode_float(const char *hex) {
    unsigned char buf[16]; from_hex(hex, buf);
    unsigned long long bits = 0;
    int i; for(i=0;i<8;i++) bits = (bits<<8)|buf[1+i];
    double val; memcpy(&val, &bits, 8);
    return val;
}

/* Encode array header */
const char *xmsgpack_encode_array_hdr(long long count) {
    unsigned char buf[8]; int pos = 0;
    if (count <= 15) { buf[pos++] = (unsigned char)(0x90 | count); }
    else if (count <= 65535) { buf[pos++] = 0xdc; buf[pos++] = (unsigned char)(count>>8); buf[pos++] = (unsigned char)count; }
    else { buf[pos++] = 0xdd; int i; for(i=3;i>=0;i--) buf[pos++]=(unsigned char)(count>>(i*8)); }
    to_hex(buf, pos, hex_buf); return hex_buf;
}

/* Encode map header */
const char *xmsgpack_encode_map_hdr(long long pairs) {
    unsigned char buf[8]; int pos = 0;
    if (pairs <= 15) { buf[pos++] = (unsigned char)(0x80 | pairs); }
    else if (pairs <= 65535) { buf[pos++] = 0xde; buf[pos++] = (unsigned char)(pairs>>8); buf[pos++] = (unsigned char)pairs; }
    else { buf[pos++] = 0xdf; int i; for(i=3;i>=0;i--) buf[pos++]=(unsigned char)(pairs>>(i*8)); }
    to_hex(buf, pos, hex_buf); return hex_buf;
}

/* Get type name */
const char *xmsgpack_type_name(const char *hex) {
    unsigned char buf[4]; from_hex(hex, buf);
    unsigned char c = buf[0];
    if (c <= 0x7f) return "int";
    if (c >= 0xe0) return "int";
    if (c >= 0x80 && c <= 0x8f) return "map";
    if (c >= 0x90 && c <= 0x9f) return "array";
    if (c >= 0xa0 && c <= 0xbf) return "str";
    if (c == 0xc0) return "nil";
    if (c == 0xc2 || c == 0xc3) return "bool";
    if (c == 0xc4 || c == 0xc5 || c == 0xc6) return "bin";
    if (c == 0xca || c == 0xcb) return "float";
    if (c >= 0xcc && c <= 0xcf) return "int";
    if (c >= 0xd0 && c <= 0xd3) return "int";
    if (c == 0xd9 || c == 0xda || c == 0xdb) return "str";
    if (c == 0xdc || c == 0xdd) return "array";
    if (c == 0xde || c == 0xdf) return "map";
    return "unknown";
}

/* Concat */
const char *xmsgpack_concat(const char *a, const char *b) {
    static char cat[16384];
    int la = (int)strlen(a);
    memcpy(cat, a, la); strcpy(cat + la, b);
    return cat;
}
