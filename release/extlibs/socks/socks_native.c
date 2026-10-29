/*
 * socks_native.c - SOCKS5 (RFC 1928) handshake codec over hex strings.
 *
 * greeting builds the method negotiation; parseChoice reads the
 * server's two-byte selection; connectDomain / connectIpv4 build
 * CONNECT requests; parseReply reads the bound-address response. All
 * binary frames cross the xlang boundary as lowercase hex. The actual
 * socket conversation runs in the xlang layer on the stdlib tcp stack.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XS_DOMAIN "xlang.extlib.socks"

static void xs_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XS_DOMAIN, 0, message);
}

static xvalue *xs_null(void) {
    return xrtValueNull();
}

static int xs_hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int xs_hex_to_bytes(const char *hex, unsigned char *out, size_t cap) {
    size_t n = strlen(hex);
    size_t i;
    if (n % 2 != 0 || n / 2 > cap) return -1;
    for (i = 0; i < n / 2; i++) {
        int hi = xs_hex_digit(hex[i * 2]);
        int lo = xs_hex_digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return (int)(n / 2);
}

static void xs_bytes_to_hex(const unsigned char *in, size_t n, char *out) {
    static const char *digits = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 0xF];
    }
    out[n * 2] = '\0';
}

const char *xs_greeting(xvalue *methods) {
    static char hex[64];
    unsigned char frame[32];
    int64 n;
    int64 i;
    if (methods == NULL || xrtValueType(methods) != XVALUE_ARRAY ||
        (n = xrtValueCount(methods)) < 1 || n > 30) {
        xs_error("greeting requires an array of 1-30 method bytes");
        hex[0] = '\0';
        return hex;
    }
    frame[0] = 0x05;
    frame[1] = (unsigned char)n;
    for (i = 0; i < n; i++) {
        xvalue *item = xrtValueArrayAt(methods, i);
        int64 value = 0;
        if (item == NULL || !xrtValueGetInt(item, &value) ||
            value < 0 || value > 255) {
            xs_error("methods must be byte values 0-255");
            hex[0] = '\0';
            return hex;
        }
        frame[2 + i] = (unsigned char)value;
    }
    xs_bytes_to_hex(frame, (size_t)n + 2, hex);
    return hex;
}

xvalue *xs_parse_choice(const char *hex) {
    unsigned char frame[2];
    xvalue *result;
    if (xs_hex_to_bytes(hex, frame, sizeof(frame)) != 2 ||
        frame[0] != 0x05) {
        xs_error("method choice must be two bytes starting with 05");
        return xs_null();
    }
    result = xrtValueObject();
    if (result == NULL) return xs_null();
    {
        xstrview view;
        view.Data = "method";
        view.Size = 6;
        xrtValueObjectSetNew(result, view, xrtValueInt(frame[1]));
    }
    return result;
}

const char *xs_connect_domain(const char *host, int port) {
    static char hex[512];
    unsigned char frame[257];
    size_t hostLen = host != NULL ? strlen(host) : 0;
    if (hostLen < 1 || hostLen > 255 || port < 0 || port > 65535) {
        xs_error("domain must be 1-255 bytes and port 0-65535");
        hex[0] = '\0';
        return hex;
    }
    frame[0] = 0x05;
    frame[1] = 0x01; /* CONNECT */
    frame[2] = 0x00; /* reserved */
    frame[3] = 0x03; /* domain */
    frame[4] = (unsigned char)hostLen;
    memcpy(frame + 5, host, hostLen);
    frame[5 + hostLen] = (unsigned char)(port >> 8);
    frame[6 + hostLen] = (unsigned char)(port & 0xFF);
    xs_bytes_to_hex(frame, hostLen + 7, hex);
    return hex;
}

const char *xs_connect_ipv4(const char *ip, int port) {
    static char hex[64];
    unsigned char frame[10];
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    if (ip == NULL || port < 0 || port > 65535) {
        xs_error("invalid ipv4 target");
        hex[0] = '\0';
        return hex;
    }
    /* manual dotted-quad parse: the Windows CRT mishandles %hhu */
    {
        const char *p = ip;
        unsigned int parts[4];
        int i;
        for (i = 0; i < 4; i++) {
            unsigned int value = 0;
            int digits = 0;
            while (*p >= '0' && *p <= '9' && digits < 3) {
                value = value * 10 + (unsigned int)(*p - '0');
                p++;
                digits++;
            }
            if (digits == 0 || value > 255) {
                xs_error("ipv4 target must be like 127.0.0.1");
                hex[0] = '\0';
                return hex;
            }
            parts[i] = value;
            if (i < 3) {
                if (*p != '.') {
                    xs_error("ipv4 target must be like 127.0.0.1");
                    hex[0] = '\0';
                    return hex;
                }
                p++;
            }
        }
        if (*p != '\0') {
            xs_error("ipv4 target must be like 127.0.0.1");
            hex[0] = '\0';
            return hex;
        }
        a = parts[0];
        b = parts[1];
        c = parts[2];
        d = parts[3];
    }
    frame[0] = 0x05;
    frame[1] = 0x01;
    frame[2] = 0x00;
    frame[3] = 0x01;
    frame[4] = (unsigned char)a;
    frame[5] = (unsigned char)b;
    frame[6] = (unsigned char)c;
    frame[7] = (unsigned char)d;
    frame[8] = (unsigned char)(port >> 8);
    frame[9] = (unsigned char)(port & 0xFF);
    xs_bytes_to_hex(frame, sizeof(frame), hex);
    return hex;
}

xvalue *xs_parse_reply(const char *hex) {
    unsigned char frame[262];
    xvalue *result;
    int len = xs_hex_to_bytes(hex, frame, sizeof(frame));
    int addrLen;
    char ip[16];
    if (len < 8 || frame[0] != 0x05) {
        xs_error("reply must start with 05 and carry an address");
        return xs_null();
    }
    if (frame[3] == 0x01) {
        addrLen = 4;
        if (len < 4 + addrLen + 2) {
            xs_error("ipv4 reply is truncated");
            return xs_null();
        }
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u",
            frame[4], frame[5], frame[6], frame[7]);
    } else if (frame[3] == 0x03) {
        addrLen = frame[4] + 1;
        if (len < 4 + addrLen + 2) {
            xs_error("domain reply is truncated");
            return xs_null();
        }
        snprintf(ip, sizeof(ip), "%.*s", (int)frame[4], frame + 5);
    } else {
        xs_error("reply address type must be ipv4 or domain");
        return xs_null();
    }
    result = xrtValueObject();
    if (result == NULL) return xs_null();
    {
        xstrview view;
        view.Data = "status";
        view.Size = 6;
        xrtValueObjectSetNew(result, view, xrtValueInt(frame[1]));
        view.Data = "bindAddress";
        view.Size = 11;
        {
            str owned = (str)xrtMalloc(strlen(ip) + 1);
            xvalue *text = NULL;
            if (owned != NULL) {
                strcpy(owned, ip);
                text = xrtValueStringTake(&owned, strlen(ip));
            }
            xrtValueObjectSetNew(result, view, text);
        }
        view.Data = "bindPort";
        view.Size = 8;
        xrtValueObjectSetNew(result, view,
            xrtValueInt((frame[4 + addrLen] << 8) | frame[5 + addrLen]));
    }
    return result;
}
