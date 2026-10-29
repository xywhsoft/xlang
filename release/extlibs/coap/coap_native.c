/* coap_native.c - CoAP (RFC 7252) packet encoding/parsing */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}
static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex)/2, i;
    for (i = 0; i < len; i++) { unsigned int v; sscanf(hex+i*2, "%02x", &v); out[i] = (unsigned char)v; }
    return len;
}

/* Build CoAP header: ver(2)|type(2)|tkl(4), code, msgid(16) */
const char *xcoap_build_header(long long msg_type, long long code, long long msg_id, long long token_len) {
    unsigned char pkt[4];
    pkt[0] = (unsigned char)(0x40 | (msg_type << 4) | (token_len & 0x0F)); /* version 1 */
    pkt[1] = (unsigned char)code;
    pkt[2] = (unsigned char)(msg_id >> 8);
    pkt[3] = (unsigned char)(msg_id & 0xFF);
    to_hex(pkt, 4, hex_buf);
    return hex_buf;
}

/* Build GET request (type=CON, code=0.01) */
const char *xcoap_get(long long msg_id, const char *path) {
    unsigned char pkt[512]; int pos = 0;
    pkt[pos++] = 0x40; /* CON, tkl=0 */
    pkt[pos++] = 0x01; /* GET */
    pkt[pos++] = (unsigned char)(msg_id >> 8);
    pkt[pos++] = (unsigned char)(msg_id & 0xFF);
    /* Uri-Path options */
    const char *p = path;
    while (*p) {
        if (*p == '/') { p++; continue; }
        const char *seg = p;
        while (*p && *p != '/') p++;
        int seglen = (int)(p - seg);
        if (seglen > 0) {
            pkt[pos++] = (unsigned char)(0xB0 | (seglen & 0x0F)); /* Uri-Path, delta=11 */
            memcpy(pkt + pos, seg, seglen); pos += seglen;
        }
    }
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build PUT request with payload */
const char *xcoap_put(long long msg_id, const char *path, const char *payload) {
    unsigned char pkt[4096]; int pos = 0;
    pkt[pos++] = 0x40; /* CON, tkl=0 */
    pkt[pos++] = 0x03; /* PUT */
    pkt[pos++] = (unsigned char)(msg_id >> 8);
    pkt[pos++] = (unsigned char)(msg_id & 0xFF);
    /* Uri-Path options */
    const char *p = path;
    while (*p) {
        if (*p == '/') { p++; continue; }
        const char *seg = p;
        while (*p && *p != '/') p++;
        int seglen = (int)(p - seg);
        if (seglen > 0) {
            pkt[pos++] = (unsigned char)(0xB0 | (seglen & 0x0F));
            memcpy(pkt + pos, seg, seglen); pos += seglen;
        }
    }
    /* Payload marker */
    int plen = (int)strlen(payload);
    if (plen > 0) {
        pkt[pos++] = 0xFF;
        memcpy(pkt + pos, payload, plen); pos += plen;
    }
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build response (e.g. 2.05 Content = 0x45) */
const char *xcoap_response(long long msg_id, long long code, const char *payload) {
    unsigned char pkt[4096]; int pos = 0;
    pkt[pos++] = 0x60; /* ACK, tkl=0 */
    pkt[pos++] = (unsigned char)code;
    pkt[pos++] = (unsigned char)(msg_id >> 8);
    pkt[pos++] = (unsigned char)(msg_id & 0xFF);
    int plen = (int)strlen(payload);
    if (plen > 0) {
        pkt[pos++] = 0xFF;
        memcpy(pkt + pos, payload, plen); pos += plen;
    }
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Get message type from hex packet */
long long xcoap_get_type(const char *pkt_hex) {
    unsigned int v; sscanf(pkt_hex, "%02x", &v);
    return (long long)((v >> 4) & 0x03);
}

/* Get code from hex packet */
long long xcoap_get_code(const char *pkt_hex) {
    unsigned int v; sscanf(pkt_hex + 2, "%02x", &v);
    return (long long)v;
}

/* Get message ID from hex packet */
long long xcoap_get_msgid(const char *pkt_hex) {
    unsigned int hi, lo;
    sscanf(pkt_hex + 4, "%02x", &hi);
    sscanf(pkt_hex + 6, "%02x", &lo);
    return (long long)((hi << 8) | lo);
}

/* Type name */
const char *xcoap_type_name(long long t) {
    switch(t) {
        case 0: return "CON";
        case 1: return "NON";
        case 2: return "ACK";
        case 3: return "RST";
        default: return "UNKNOWN";
    }
}

/* Code name */
const char *xcoap_code_name(long long code) {
    switch(code) {
        case 0x01: return "GET";
        case 0x02: return "POST";
        case 0x03: return "PUT";
        case 0x04: return "DELETE";
        case 0x45: return "2.05 Content";
        case 0x41: return "2.01 Created";
        case 0x42: return "2.02 Deleted";
        case 0x43: return "2.03 Valid";
        case 0x44: return "2.04 Changed";
        case 0x80: return "4.00 Bad Request";
        case 0x84: return "4.04 Not Found";
        case 0xA0: return "5.00 Internal Error";
        default: return "UNKNOWN";
    }
}

/* Validate CoAP path */
long long xcoap_valid_path(const char *path) {
    int len = (int)strlen(path);
    if (len < 1 || len > 255) return 0;
    if (path[0] != '/') return 0;
    return 1;
}

/* Default CoAP port */
long long xcoap_default_port(void) { return 5683; }

/* Default CoAP secure port */
long long xcoap_dtls_port(void) { return 5684; }
