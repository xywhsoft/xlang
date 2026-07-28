/* nanomsg_native.c - nanomsg protocol helpers (SP protocol framing) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* SP protocol header: type(16bit) + flags(16bit) */
/* Build SP message header */
const char *xnn_header(long long proto, long long flags) {
    unsigned char pkt[8]; int pos = 0;
    /* SP header: 0x00 + SP version (0x53 0x50 = "SP") */
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x53; /* 'S' */
    pkt[pos++] = 0x50; /* 'P' */
    pkt[pos++] = 0x00;
    /* Protocol type (16-bit big-endian) */
    pkt[pos++] = (unsigned char)(proto >> 8);
    pkt[pos++] = (unsigned char)(proto & 0xFF);
    /* Flags (16-bit) */
    pkt[pos++] = (unsigned char)(flags >> 8);
    pkt[pos++] = (unsigned char)(flags & 0xFF);
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build complete message with header + body */
const char *xnn_message(long long proto, const char *body) {
    unsigned char pkt[4096]; int pos = 0;
    int blen = (int)strlen(body);
    /* SP header */
    pkt[pos++] = 0x00; pkt[pos++] = 0x53; pkt[pos++] = 0x50; pkt[pos++] = 0x00;
    pkt[pos++] = (unsigned char)(proto >> 8);
    pkt[pos++] = (unsigned char)(proto & 0xFF);
    pkt[pos++] = 0x00; pkt[pos++] = 0x00; /* flags */
    memcpy(pkt + pos, body, blen); pos += blen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Get protocol type from hex message */
long long xnn_get_proto(const char *msg_hex) {
    unsigned int hi, lo;
    sscanf(msg_hex + 8, "%02x", &hi);  /* offset 4 bytes = 8 hex chars */
    sscanf(msg_hex + 10, "%02x", &lo);
    return (long long)((hi << 8) | lo);
}

/* Get body from hex message (skip 8-byte header) */
const char *xnn_get_body(const char *msg_hex) {
    int hlen = (int)strlen(msg_hex);
    int body_start = 16; /* 8 bytes header = 16 hex chars */
    int body_hex_len = hlen - body_start;
    int body_len = body_hex_len / 2, i;
    for (i = 0; i < body_len; i++) {
        unsigned int v; sscanf(msg_hex + body_start + i*2, "%02x", &v);
        str_buf[i] = (char)v;
    }
    str_buf[body_len] = '\0';
    return str_buf;
}

/* Protocol names (nanomsg scalability protocols) */
const char *xnn_proto_name(long long proto) {
    switch(proto) {
        case 0x0010: return "PAIR";
        case 0x0020: return "PUB";
        case 0x0021: return "SUB";
        case 0x0030: return "REQ";
        case 0x0031: return "REP";
        case 0x0050: return "PUSH";
        case 0x0051: return "PULL";
        case 0x0060: return "SURVEYOR";
        case 0x0061: return "RESPONDENT";
        case 0x0070: return "BUS";
        default: return "UNKNOWN";
    }
}

/* Transport names */
const char *xnn_transport_name(long long transport) {
    switch(transport) {
        case 0: return "inproc";
        case 1: return "ipc";
        case 2: return "tcp";
        case 3: return "ws";
        default: return "UNKNOWN";
    }
}

/* Build address string */
const char *xnn_address(long long transport, const char *addr, long long port) {
    const char *tname = xnn_transport_name(transport);
    if (transport == 2) { /* tcp */
        sprintf(str_buf, "tcp://%s:%lld", addr, port);
    } else if (transport == 3) { /* ws */
        sprintf(str_buf, "ws://%s:%lld", addr, port);
    } else {
        sprintf(str_buf, "%s://%s", tname, addr);
    }
    return str_buf;
}

/* Validate protocol number */
long long xnn_valid_proto(long long proto) {
    switch(proto) {
        case 0x0010: case 0x0020: case 0x0021:
        case 0x0030: case 0x0031:
        case 0x0050: case 0x0051:
        case 0x0060: case 0x0061:
        case 0x0070:
            return 1;
        default: return 0;
    }
}

/* nanomsg version */
long long xnn_version(void) { return 1; }

/* Default TCP port */
long long xnn_default_port(void) { return 5555; }
