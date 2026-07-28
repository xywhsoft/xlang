/* snmp_native.c - SNMP protocol helpers (BER/TLV encoding, OID handling) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* SNMP version name */
const char* xsnmp_version_name(long long ver) {
    switch (ver) {
        case 0: return "v1";
        case 1: return "v2c";
        case 3: return "v3";
        default: return "unknown";
    }
}

/* PDU type name */
const char* xsnmp_pdu_name(long long pdu_type) {
    switch (pdu_type) {
        case 0xA0: return "GetRequest";
        case 0xA1: return "GetNextRequest";
        case 0xA2: return "Response";
        case 0xA3: return "SetRequest";
        case 0xA4: return "Trap";
        case 0xA5: return "GetBulkRequest";
        case 0xA6: return "InformRequest";
        case 0xA7: return "TrapV2";
        default: return "Unknown";
    }
}

/* BER type name */
const char* xsnmp_ber_name(long long tag) {
    switch (tag) {
        case 0x02: return "INTEGER";
        case 0x04: return "OCTET_STRING";
        case 0x05: return "NULL";
        case 0x06: return "OBJECT_IDENTIFIER";
        case 0x30: return "SEQUENCE";
        case 0x40: return "IpAddress";
        case 0x41: return "Counter32";
        case 0x42: return "Gauge32";
        case 0x43: return "TimeTicks";
        case 0x44: return "Opaque";
        case 0x46: return "Counter64";
        default: return "Unknown";
    }
}

/* Encode OID string "1.3.6.1.2.1" to hex BER encoding */
const char* xsnmp_oid_encode(const char *oid_str) {
    unsigned int parts[64]; int n = 0;
    const char *p = oid_str;
    while (*p && n < 64) {
        parts[n++] = (unsigned int)atoi(p);
        while (*p && *p != '.') p++;
        if (*p == '.') p++;
    }
    if (n < 2) return "";
    unsigned char enc[256]; int elen = 0;
    /* First two arcs combined: 40*X + Y */
    unsigned int first = parts[0] * 40 + parts[1];
    /* Encode each sub-identifier in base-128 */
    unsigned int vals[64]; int vn = 0;
    vals[vn++] = first;
    for (int i = 2; i < n; i++) vals[vn++] = parts[i];
    for (int i = 0; i < vn; i++) {
        unsigned int v = vals[i];
        unsigned char tmp[8]; int tlen = 0;
        tmp[tlen++] = v & 0x7F;
        v >>= 7;
        while (v > 0) { tmp[tlen++] = (v & 0x7F) | 0x80; v >>= 7; }
        for (int j = tlen - 1; j >= 0; j--) enc[elen++] = tmp[j];
    }
    /* Format as hex with tag 0x06 and length */
    int pos = 0;
    pos += sprintf(str_buf + pos, "06");
    if (elen < 128) pos += sprintf(str_buf + pos, "%02x", elen);
    else { pos += sprintf(str_buf + pos, "81%02x", elen); }
    for (int i = 0; i < elen; i++) pos += sprintf(str_buf + pos, "%02x", enc[i]);
    return str_buf;
}

/* Decode BER OID hex (without tag/length) back to dotted string */
const char* xsnmp_oid_decode(const char *oid_hex) {
    unsigned int bytes[256]; int n = 0;
    const char *p = oid_hex;
    while (*p && n < 256) {
        unsigned int v;
        if (sscanf(p, "%02x", &v) != 1) break;
        bytes[n++] = v;
        p += 2;
    }
    /* Decode base-128 sub-identifiers */
    unsigned int arcs[64]; int an = 0;
    unsigned int cur = 0;
    for (int i = 0; i < n; i++) {
        cur = (cur << 7) | (bytes[i] & 0x7F);
        if (!(bytes[i] & 0x80)) {
            arcs[an++] = cur;
            cur = 0;
        }
    }
    /* First arc splits into two */
    int pos = 0;
    if (an > 0) {
        unsigned int first = arcs[0];
        if (first < 40) pos += sprintf(str_buf + pos, "0.%u", first);
        else if (first < 80) pos += sprintf(str_buf + pos, "1.%u", first - 40);
        else pos += sprintf(str_buf + pos, "2.%u", first - 80);
    }
    for (int i = 1; i < an; i++) {
        pos += sprintf(str_buf + pos, ".%u", arcs[i]);
    }
    return str_buf;
}

/* Encode integer as BER hex */
const char* xsnmp_int_encode(long long val) {
    unsigned char enc[16]; int elen = 0;
    long long v = val;
    if (v == 0) { enc[elen++] = 0; }
    else if (v > 0) {
        unsigned char tmp[8]; int tlen = 0;
        while (v > 0) { tmp[tlen++] = v & 0xFF; v >>= 8; }
        if (tmp[tlen-1] & 0x80) tmp[tlen++] = 0;
        for (int i = tlen - 1; i >= 0; i--) enc[elen++] = tmp[i];
    } else {
        /* Negative: two's complement minimal encoding */
        int done = 0;
        for (int i = 7; i >= 0 && !done; i--) {
            unsigned char b = (v >> (i * 8)) & 0xFF;
            unsigned char sign = (v < 0) ? 0xFF : 0x00;
            if (b != sign || i == 0) {
                for (int j = i; j >= 0; j--) enc[elen++] = (v >> (j*8)) & 0xFF;
                done = 1;
            }
        }
    }
    int pos = sprintf(str_buf, "02%02x", elen);
    for (int i = 0; i < elen; i++) pos += sprintf(str_buf + pos, "%02x", enc[i]);
    return str_buf;
}

/* Decode BER integer from hex (without tag/length prefix) */
long long xsnmp_int_decode(const char *hex) {
    unsigned int bytes[16]; int n = 0;
    const char *p = hex;
    while (*p && n < 16) {
        unsigned int v;
        if (sscanf(p, "%02x", &v) != 1) break;
        bytes[n++] = v;
        p += 2;
    }
    if (n == 0) return 0;
    long long val = (bytes[0] & 0x80) ? -1 : 0;
    for (int i = 0; i < n; i++) val = (val << 8) | bytes[i];
    return val;
}

/* SNMP error status name */
const char* xsnmp_error_name(long long err) {
    switch (err) {
        case 0: return "noError";
        case 1: return "tooBig";
        case 2: return "noSuchName";
        case 3: return "badValue";
        case 4: return "readOnly";
        case 5: return "genErr";
        case 6: return "noAccess";
        case 7: return "wrongType";
        case 8: return "wrongLength";
        case 9: return "wrongEncoding";
        case 10: return "wrongValue";
        case 11: return "noCreation";
        case 12: return "inconsistentValue";
        case 13: return "resourceUnavailable";
        case 14: return "commitFailed";
        case 15: return "undoFailed";
        case 16: return "authorizationError";
        case 17: return "notWritable";
        case 18: return "inconsistentName";
        default: return "unknown";
    }
}

/* Common OID names */
const char* xsnmp_oid_name(const char *oid) {
    if (strcmp(oid, "1.3.6.1.2.1.1.1") == 0) return "sysDescr";
    if (strcmp(oid, "1.3.6.1.2.1.1.2") == 0) return "sysObjectID";
    if (strcmp(oid, "1.3.6.1.2.1.1.3") == 0) return "sysUpTime";
    if (strcmp(oid, "1.3.6.1.2.1.1.4") == 0) return "sysContact";
    if (strcmp(oid, "1.3.6.1.2.1.1.5") == 0) return "sysName";
    if (strcmp(oid, "1.3.6.1.2.1.1.6") == 0) return "sysLocation";
    if (strcmp(oid, "1.3.6.1.2.1.2.1") == 0) return "ifNumber";
    if (strcmp(oid, "1.3.6.1.2.1.2.2.1.10") == 0) return "ifInOctets";
    if (strcmp(oid, "1.3.6.1.2.1.2.2.1.16") == 0) return "ifOutOctets";
    return "unknown";
}
