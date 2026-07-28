/* avro_native.c - Apache Avro format helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Avro type name from type code */
const char* xavro_type_name(long long type_code) {
    switch (type_code) {
        case 0: return "null";
        case 1: return "boolean";
        case 2: return "int";
        case 3: return "long";
        case 4: return "float";
        case 5: return "double";
        case 6: return "bytes";
        case 7: return "string";
        case 8: return "record";
        case 9: return "enum";
        case 10: return "array";
        case 11: return "map";
        case 12: return "union";
        case 13: return "fixed";
        default: return "unknown";
    }
}

/* Avro logical type name */
const char* xavro_logical_name(long long code) {
    switch (code) {
        case 0: return "decimal";
        case 1: return "date";
        case 2: return "time-millis";
        case 3: return "time-micros";
        case 4: return "timestamp-millis";
        case 5: return "timestamp-micros";
        case 6: return "duration";
        case 7: return "uuid";
        default: return "unknown";
    }
}

/* Encode zigzag (Avro varint encoding for signed integers) */
long long xavro_zigzag_encode(long long n) {
    return (n << 1) ^ (n >> 63);
}

/* Decode zigzag */
long long xavro_zigzag_decode(long long n) {
    return (n >> 1) ^ -(n & 1);
}

/* Encode long as Avro varint hex string */
const char* xavro_encode_long(long long val) {
    unsigned long long n = (unsigned long long)xavro_zigzag_encode(val);
    int pos = 0;
    while (n > 0x7F) {
        pos += sprintf(str_buf + pos, "%02x", (int)((n & 0x7F) | 0x80));
        n >>= 7;
    }
    pos += sprintf(str_buf + pos, "%02x", (int)(n & 0x7F));
    return str_buf;
}

/* Decode Avro varint from hex string, returns decoded value */
long long xavro_decode_long(const char *hex) {
    unsigned long long result = 0;
    int shift = 0;
    const char *p = hex;
    while (*p) {
        unsigned int byte;
        if (sscanf(p, "%02x", &byte) != 1) break;
        result |= (unsigned long long)(byte & 0x7F) << shift;
        shift += 7;
        p += 2;
        if (!(byte & 0x80)) break;
    }
    return xavro_zigzag_decode((long long)result);
}

/* Encode string as Avro bytes (length-prefixed): returns hex */
const char* xavro_encode_string(const char *s) {
    int len = (int)strlen(s);
    /* Encode length as zigzag varint */
    unsigned long long n = (unsigned long long)(len << 1); /* zigzag for positive = val*2 */
    int pos = 0;
    while (n > 0x7F) {
        pos += sprintf(str_buf + pos, "%02x", (int)((n & 0x7F) | 0x80));
        n >>= 7;
    }
    pos += sprintf(str_buf + pos, "%02x", (int)(n & 0x7F));
    /* Append string bytes as hex */
    for (int i = 0; i < len; i++) {
        pos += sprintf(str_buf + pos, "%02x", (unsigned char)s[i]);
    }
    return str_buf;
}

/* Decode Avro string from hex, returns the string */
const char* xavro_decode_string(const char *hex) {
    const char *p = hex;
    unsigned long long len = 0;
    int shift = 0;
    /* Read varint length */
    while (*p) {
        unsigned int byte;
        if (sscanf(p, "%02x", &byte) != 1) break;
        len |= (unsigned long long)(byte & 0x7F) << shift;
        shift += 7;
        p += 2;
        if (!(byte & 0x80)) break;
    }
    len >>= 1; /* zigzag decode for positive */
    /* Read string bytes */
    int slen = 0;
    for (unsigned long long i = 0; i < len && *p; i++) {
        unsigned int byte;
        if (sscanf(p, "%02x", &byte) != 1) break;
        str_buf[slen++] = (char)byte;
        p += 2;
    }
    str_buf[slen] = '\0';
    return str_buf;
}

/* Avro container file magic bytes as hex */
const char* xavro_magic(void) {
    return "4f626a01";
}

/* Codec name */
const char* xavro_codec_name(long long codec) {
    switch (codec) {
        case 0: return "null";
        case 1: return "deflate";
        case 2: return "snappy";
        case 3: return "zstandard";
        case 4: return "bzip2";
        case 5: return "xz";
        default: return "unknown";
    }
}

/* Sort order name */
const char* xavro_sort_order(long long order) {
    switch (order) {
        case 0: return "ascending";
        case 1: return "descending";
        case 2: return "ignore";
        default: return "unknown";
    }
}

/* Validate Avro name (must match [A-Za-z_][A-Za-z0-9_]*) */
long long xavro_valid_name(const char *name) {
    int len = (int)strlen(name);
    if (len == 0) return 0;
    char c = name[0];
    if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') && c != '_') return 0;
    for (int i = 1; i < len; i++) {
        c = name[i];
        if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9') && c != '_') return 0;
    }
    return 1;
}
