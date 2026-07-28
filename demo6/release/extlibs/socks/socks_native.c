/* socks_native.c - SOCKS5 protocol packet construction and parsing */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[4096];
static char err_buf[256];

const char *xsocks_error(void) { return err_buf; }

/* SOCKS5 version */
long long xsocks_version(void) { return 5; }

/* Build greeting packet (client -> server): VER, NMETHODS, METHODS */
/* Returns hex-encoded packet, returns byte count */
long long xsocks_build_greeting(long long use_auth) {
    unsigned char pkt[4];
    int len;
    pkt[0] = 0x05; /* VER */
    if (use_auth) {
        pkt[1] = 2; /* NMETHODS */
        pkt[2] = 0x00; /* NO AUTH */
        pkt[3] = 0x02; /* USERNAME/PASSWORD */
        len = 4;
    } else {
        pkt[1] = 1;
        pkt[2] = 0x00;
        len = 3;
    }
    int i;
    for (i = 0; i < len; i++)
        sprintf(buf + i * 2, "%02x", pkt[i]);
    buf[len * 2] = '\0';
    return len;
}

/* Build connect request: VER CMD RSV ATYP DST.ADDR DST.PORT */
long long xsocks_build_connect(const char *host, long long port) {
    unsigned char pkt[264];
    int pos = 0;
    pkt[pos++] = 0x05; /* VER */
    pkt[pos++] = 0x01; /* CMD: CONNECT */
    pkt[pos++] = 0x00; /* RSV */

    /* Try to parse as IP address */
    int a, b, c, d;
    if (sscanf(host, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        pkt[pos++] = 0x01; /* ATYP: IPv4 */
        pkt[pos++] = (unsigned char)a;
        pkt[pos++] = (unsigned char)b;
        pkt[pos++] = (unsigned char)c;
        pkt[pos++] = (unsigned char)d;
    } else {
        pkt[pos++] = 0x03; /* ATYP: DOMAINNAME */
        int hlen = (int)strlen(host);
        if (hlen > 255) hlen = 255;
        pkt[pos++] = (unsigned char)hlen;
        memcpy(pkt + pos, host, hlen);
        pos += hlen;
    }

    pkt[pos++] = (unsigned char)((port >> 8) & 0xFF);
    pkt[pos++] = (unsigned char)(port & 0xFF);

    int i;
    for (i = 0; i < pos; i++)
        sprintf(buf + i * 2, "%02x", pkt[i]);
    buf[pos * 2] = '\0';
    return pos;
}

/* Build username/password auth packet */
long long xsocks_build_auth(const char *user, const char *pass) {
    unsigned char pkt[513];
    int pos = 0;
    int ulen = (int)strlen(user);
    int plen = (int)strlen(pass);
    if (ulen > 255) ulen = 255;
    if (plen > 255) plen = 255;
    pkt[pos++] = 0x01; /* auth version */
    pkt[pos++] = (unsigned char)ulen;
    memcpy(pkt + pos, user, ulen); pos += ulen;
    pkt[pos++] = (unsigned char)plen;
    memcpy(pkt + pos, pass, plen); pos += plen;

    int i;
    for (i = 0; i < pos; i++)
        sprintf(buf + i * 2, "%02x", pkt[i]);
    buf[pos * 2] = '\0';
    return pos;
}

/* Parse greeting response: returns method (0=no auth, 2=user/pass, 0xFF=fail) */
long long xsocks_parse_greeting(const char *hex_pkt, long long len) {
    if (len < 2) return -1;
    /* Parse hex to get method byte */
    char byte_str[3] = {hex_pkt[2], hex_pkt[3], '\0'};
    return strtol(byte_str, NULL, 16);
}

/* Parse connect response: returns reply code (0=success) */
long long xsocks_parse_reply(const char *hex_pkt, long long len) {
    if (len < 3) return -1;
    char byte_str[3] = {hex_pkt[2], hex_pkt[3], '\0'};
    return strtol(byte_str, NULL, 16);
}

/* SOCKS5 reply codes */
const char *xsocks_reply_str(long long code) {
    switch (code) {
        case 0: return "succeeded";
        case 1: return "general failure";
        case 2: return "connection not allowed";
        case 3: return "network unreachable";
        case 4: return "host unreachable";
        case 5: return "connection refused";
        case 6: return "TTL expired";
        case 7: return "command not supported";
        case 8: return "address type not supported";
        default: return "unknown";
    }
}

/* Validate a SOCKS5 greeting response */
long long xsocks_valid_greeting(const char *hex_pkt, long long len) {
    if (len < 2) return 0;
    /* First byte should be 05 */
    if (hex_pkt[0] != '0' || hex_pkt[1] != '5') return 0;
    return 1;
}
