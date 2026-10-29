/* mdns_native.c - mDNS packet construction and parsing utilities */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[4096];
static char err_buf[256];

const char *xmdns_error(void) { return err_buf; }

/* Encode a DNS name into wire format (labels) */
const char *xmdns_encode_name(const char *name) {
    int pos = 0;
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int len = dot ? (int)(dot - p) : (int)strlen(p);
        if (len > 63) len = 63;
        buf[pos++] = (char)len;
        memcpy(buf + pos, p, len);
        pos += len;
        p = dot ? dot + 1 : p + strlen(p);
    }
    buf[pos++] = '\0'; /* root label */
    buf[pos] = '\0';
    return buf;
}

/* Decode a DNS wire-format name back to dotted string */
const char *xmdns_decode_name(const char *wire) {
    int pos = 0;
    int out = 0;
    static char out_buf[256];
    while (wire[pos] != '\0') {
        int len = (unsigned char)wire[pos];
        pos++;
        if (out > 0) out_buf[out++] = '.';
        memcpy(out_buf + out, wire + pos, len);
        out += len;
        pos += len;
    }
    out_buf[out] = '\0';
    return out_buf;
}

/* Build a minimal mDNS query packet for a service name */
/* Returns hex-encoded packet */
long long xmdns_build_query(const char *name, long long qtype) {
    /* DNS header: ID=0, QR=0, OPCODE=0, QDCOUNT=1 */
    unsigned char pkt[512];
    int pos = 0;
    memset(pkt, 0, 12); /* header */
    pkt[5] = 1; /* QDCOUNT = 1 */
    pos = 12;

    /* Encode name */
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int len = dot ? (int)(dot - p) : (int)strlen(p);
        pkt[pos++] = (unsigned char)len;
        memcpy(pkt + pos, p, len);
        pos += len;
        p = dot ? dot + 1 : p + strlen(p);
    }
    pkt[pos++] = 0; /* root */

    /* QTYPE */
    pkt[pos++] = (unsigned char)((qtype >> 8) & 0xFF);
    pkt[pos++] = (unsigned char)(qtype & 0xFF);
    /* QCLASS = IN (1) with unicast-response bit */
    pkt[pos++] = 0x80;
    pkt[pos++] = 0x01;

    /* Convert to hex string */
    int i;
    for (i = 0; i < pos; i++)
        sprintf(buf + i * 2, "%02x", pkt[i]);
    buf[pos * 2] = '\0';
    return pos;
}

/* Get mDNS multicast address */
const char *xmdns_multicast_addr(void) {
    return "224.0.0.251";
}

/* Get mDNS port */
long long xmdns_port(void) {
    return 5353;
}

/* Common service types */
const char *xmdns_service_http(void) { return "_http._tcp.local"; }
const char *xmdns_service_ssh(void) { return "_ssh._tcp.local"; }
const char *xmdns_service_dns(void) { return "_dns._udp.local"; }

/* Validate a service name format */
long long xmdns_valid_name(const char *name) {
    if (!name || strlen(name) == 0) return 0;
    if (strlen(name) > 255) return 0;
    /* Must contain at least one dot */
    if (!strchr(name, '.')) return 0;
    /* Labels must be 1-63 chars */
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int len = dot ? (int)(dot - p) : (int)strlen(p);
        if (len < 1 || len > 63) return 0;
        p = dot ? dot + 1 : p + strlen(p);
    }
    return 1;
}

/* DNS record types */
long long xmdns_type_a(void) { return 1; }
long long xmdns_type_aaaa(void) { return 28; }
long long xmdns_type_ptr(void) { return 12; }
long long xmdns_type_srv(void) { return 33; }
long long xmdns_type_txt(void) { return 16; }
