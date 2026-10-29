/* zeromq_native.c - ZeroMQ message framing and pattern helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* ZMTP 3.1 frame: flags(1) + size(1 or 8) + body */
/* Build a single-part message frame */
const char *xzmq_frame(const char *body, long long more) {
    unsigned char pkt[4096]; int pos = 0;
    int blen = (int)strlen(body);
    unsigned char flags = 0;
    if (more) flags |= 0x01; /* MORE flag */
    if (blen > 255) {
        flags |= 0x02; /* LONG flag */
        pkt[pos++] = flags;
        /* 8-byte size (only lower 4 used) */
        pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0;
        pkt[pos++] = (unsigned char)(blen >> 24);
        pkt[pos++] = (unsigned char)(blen >> 16);
        pkt[pos++] = (unsigned char)(blen >> 8);
        pkt[pos++] = (unsigned char)(blen & 0xFF);
    } else {
        pkt[pos++] = flags;
        pkt[pos++] = (unsigned char)blen;
    }
    memcpy(pkt + pos, body, blen); pos += blen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build multi-part message (2 parts) */
const char *xzmq_multipart(const char *part1, const char *part2) {
    unsigned char pkt[8192]; int pos = 0;
    int len1 = (int)strlen(part1);
    int len2 = (int)strlen(part2);
    /* Part 1 with MORE flag */
    pkt[pos++] = 0x01; /* MORE */
    pkt[pos++] = (unsigned char)len1;
    memcpy(pkt + pos, part1, len1); pos += len1;
    /* Part 2 final */
    pkt[pos++] = 0x00;
    pkt[pos++] = (unsigned char)len2;
    memcpy(pkt + pos, part2, len2); pos += len2;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Get frame body from hex (first frame only) */
const char *xzmq_frame_body(const char *frame_hex) {
    unsigned char pkt[4096]; int len = (int)strlen(frame_hex)/2, i;
    for (i = 0; i < len && i < 4096; i++) { unsigned int v; sscanf(frame_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    unsigned char flags = pkt[0];
    int body_off, body_len;
    if (flags & 0x02) { /* LONG */
        body_len = (pkt[5] << 24) | (pkt[6] << 16) | (pkt[7] << 8) | pkt[8];
        body_off = 9;
    } else {
        body_len = pkt[1];
        body_off = 2;
    }
    memcpy(str_buf, pkt + body_off, body_len);
    str_buf[body_len] = '\0';
    return str_buf;
}

/* Check if frame has MORE flag */
long long xzmq_has_more(const char *frame_hex) {
    unsigned int v; sscanf(frame_hex, "%02x", &v);
    return (long long)(v & 0x01);
}

/* Get frame body length */
long long xzmq_frame_len(const char *frame_hex) {
    unsigned char pkt[16]; int i;
    for (i = 0; i < 9; i++) { unsigned int v; sscanf(frame_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    if (pkt[0] & 0x02) {
        return (long long)((pkt[5] << 24) | (pkt[6] << 16) | (pkt[7] << 8) | pkt[8]);
    }
    return (long long)pkt[1];
}

/* Socket type names */
const char *xzmq_socket_name(long long stype) {
    switch(stype) {
        case 0: return "PAIR";
        case 1: return "PUB";
        case 2: return "SUB";
        case 3: return "REQ";
        case 4: return "REP";
        case 5: return "DEALER";
        case 6: return "ROUTER";
        case 7: return "PULL";
        case 8: return "PUSH";
        case 9: return "XPUB";
        case 10: return "XSUB";
        default: return "UNKNOWN";
    }
}

/* Transport protocol names */
const char *xzmq_transport_name(long long proto) {
    switch(proto) {
        case 0: return "tcp";
        case 1: return "ipc";
        case 2: return "inproc";
        case 3: return "pgm";
        case 4: return "epgm";
        default: return "UNKNOWN";
    }
}

/* Build endpoint string */
const char *xzmq_endpoint(long long proto, const char *addr, long long port) {
    const char *pname = xzmq_transport_name(proto);
    if (proto == 0) { /* tcp */
        sprintf(str_buf, "tcp://%s:%lld", addr, port);
    } else {
        sprintf(str_buf, "%s://%s", pname, addr);
    }
    return str_buf;
}

/* Validate socket type for pattern */
long long xzmq_valid_pattern(long long pattern) {
    /* 0=REQ-REP, 1=PUB-SUB, 2=PUSH-PULL, 3=PAIR */
    return (pattern >= 0 && pattern <= 3) ? 1 : 0;
}

/* Pattern name */
const char *xzmq_pattern_name(long long pattern) {
    switch(pattern) {
        case 0: return "REQ-REP";
        case 1: return "PUB-SUB";
        case 2: return "PUSH-PULL";
        case 3: return "PAIR";
        default: return "UNKNOWN";
    }
}

/* ZMTP version */
long long xzmq_zmtp_version(void) { return 3; }

/* Default TCP port for ZMQ */
long long xzmq_default_port(void) { return 5555; }
