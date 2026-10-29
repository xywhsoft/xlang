/*
 * ntp_native.c - NTP (RFC 5905 subset) packet codec over hex strings.
 *
 * Binary packets cross the xlang boundary as lowercase hex so no byte
 * transport is needed. buildRequest emits a 48-byte client poll (NTP
 * version 4, mode 3). parseResponse reads the fixed header plus the
 * transmit timestamp and converts it to Unix seconds. Connections run
 * in the xlang layer over the stdlib udp namespace.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XN_DOMAIN "xlang.extlib.ntp"

#define NTP_UNIX_OFFSET 2208988800.0

static void xn_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XN_DOMAIN, 0, message);
}

static xvalue *xn_null(void) {
    return xrtValueNull();
}

static xvalue *xn_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static int xn_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

static int xn_hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int xn_hex_to_bytes(const char *hex, unsigned char *out, size_t cap) {
    size_t n = strlen(hex);
    size_t i;
    if (n % 2 != 0 || n / 2 > cap) return -1;
    for (i = 0; i < n / 2; i++) {
        int hi = xn_hex_digit(hex[i * 2]);
        int lo = xn_hex_digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return (int)(n / 2);
}

static void xn_bytes_to_hex(const unsigned char *in, size_t n, char *out) {
    static const char *digits = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 0xF];
    }
    out[n * 2] = '\0';
}

const char *xn_build_request(void) {
    static char hex[97];
    unsigned char packet[48];
    memset(packet, 0, sizeof(packet));
    packet[0] = 0x23; /* LI=0, VN=4, Mode=3 (client) */
    xn_bytes_to_hex(packet, sizeof(packet), hex);
    return hex;
}

xvalue *xn_parse_response(const char *hex) {
    unsigned char packet[48];
    xvalue *result;
    int len = xn_hex_to_bytes(hex, packet, sizeof(packet));
    unsigned int seconds;
    unsigned int fraction;
    double unixTime;
    if (len != 48) {
        xn_error("response must be a 48-byte packet in hex");
        return xn_null();
    }
    seconds = ((unsigned int)packet[40] << 24) |
              ((unsigned int)packet[41] << 16) |
              ((unsigned int)packet[42] << 8) |
              (unsigned int)packet[43];
    fraction = ((unsigned int)packet[44] << 24) |
               ((unsigned int)packet[45] << 16) |
               ((unsigned int)packet[46] << 8) |
               (unsigned int)packet[47];
    unixTime = (double)seconds - NTP_UNIX_OFFSET +
        (double)fraction / 4294967296.0;
    result = xrtValueObject();
    if (result == NULL) return xn_null();
    xn_set(result, "leapIndicator", xrtValueInt(packet[0] >> 6));
    xn_set(result, "version", xrtValueInt((packet[0] >> 3) & 0x07));
    xn_set(result, "mode", xrtValueInt(packet[0] & 0x07));
    xn_set(result, "stratum", xrtValueInt(packet[1]));
    xn_set(result, "ntpSeconds", xrtValueInt((int64)seconds));
    xn_set(result, "unixSeconds", xrtValueFloat(unixTime));
    return result;
}

long long xn_unix_offset(void) {
    return 2208988800LL;
}
