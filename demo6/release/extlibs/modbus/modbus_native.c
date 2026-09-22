/*
 * modbus_native.c - MODBUS/TCP (MBAP) frame codec over hex strings.
 *
 * buildRead/buildWrite emit request frames with the MBAP header
 * (transaction, protocol 0, length, unit) plus the function PDU.
 * parseResponse reads register values, write echoes, and exception
 * frames. Binary frames cross the xlang boundary as lowercase hex;
 * bytesToFrame returns a raw buffer pointer the xlang side wraps in
 * stdlib Buffer, and frameToBytesHex converts received raw memory
 * back to hex.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XM_DOMAIN "xlang.extlib.modbus"

static void xm_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XM_DOMAIN, 0, message);
}

static xvalue *xm_null(void) {
    return xrtValueNull();
}

static int xm_hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int xm_hex_to_bytes(const char *hex, unsigned char *out, size_t cap) {
    size_t n = strlen(hex);
    size_t i;
    if (n % 2 != 0 || n / 2 > cap) return -1;
    for (i = 0; i < n / 2; i++) {
        int hi = xm_hex_digit(hex[i * 2]);
        int lo = xm_hex_digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return (int)(n / 2);
}

static void xm_bytes_to_hex(const unsigned char *in, size_t n, char *out) {
    static const char *digits = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 0xF];
    }
    out[n * 2] = '\0';
}

/* Validate MBAP header; returns the declared length or -1. */
static int xm_mbap_check(const unsigned char *f, int len) {
    int declared;
    if (len < 8) return -1;
    if (f[2] != 0 || f[3] != 0) return -1;
    declared = (f[4] << 8) | f[5];
    if (declared < 2 || 6 + declared > len + 2) return -1;
    return declared;
}

const char *xm_build_read(long long transaction, long long unit,
    long long startAddr, long long quantity, int function) {
    static char hex[32];
    unsigned char f[12];
    if (transaction < 0 || transaction > 0xFFFF || unit < 0 ||
        unit > 0xFF || startAddr < 0 || startAddr > 0xFFFF ||
        quantity < 1 || quantity > 125) {
        xm_error("read parameters out of range");
        hex[0] = '\0';
        return hex;
    }
    f[0] = (unsigned char)(transaction >> 8);
    f[1] = (unsigned char)(transaction & 0xFF);
    f[2] = 0;
    f[3] = 0;
    f[4] = 0;
    f[5] = 6;
    f[6] = (unsigned char)unit;
    f[7] = (unsigned char)function;
    f[8] = (unsigned char)(startAddr >> 8);
    f[9] = (unsigned char)(startAddr & 0xFF);
    f[10] = (unsigned char)(quantity >> 8);
    f[11] = (unsigned char)(quantity & 0xFF);
    xm_bytes_to_hex(f, sizeof(f), hex);
    return hex;
}

const char *xm_build_write_single(long long transaction, long long unit,
    long long addr, long long value) {
    static char hex[32];
    unsigned char f[12];
    if (transaction < 0 || transaction > 0xFFFF || unit < 0 ||
        unit > 0xFF || addr < 0 || addr > 0xFFFF || value < 0 ||
        value > 0xFFFF) {
        xm_error("write parameters out of range");
        hex[0] = '\0';
        return hex;
    }
    f[0] = (unsigned char)(transaction >> 8);
    f[1] = (unsigned char)(transaction & 0xFF);
    f[2] = 0;
    f[3] = 0;
    f[4] = 0;
    f[5] = 6;
    f[6] = (unsigned char)unit;
    f[7] = 0x06;
    f[8] = (unsigned char)(addr >> 8);
    f[9] = (unsigned char)(addr & 0xFF);
    f[10] = (unsigned char)(value >> 8);
    f[11] = (unsigned char)(value & 0xFF);
    xm_bytes_to_hex(f, sizeof(f), hex);
    return hex;
}

xvalue *xm_parse_response(const char *hex) {
    unsigned char f[263];
    xvalue *result;
    int len = xm_hex_to_bytes(hex, f, sizeof(f));
    int function;
    if (len < 0 || xm_mbap_check(f, len) < 0) {
        xm_error("response is not a valid mbap frame");
        return xm_null();
    }
    function = f[7];
    result = xrtValueObject();
    if (result == NULL) return xm_null();
    {
        xstrview view;
        view.Data = "transaction";
        view.Size = 11;
        xrtValueObjectSetNew(result, view, xrtValueInt((f[0] << 8) | f[1]));
        view.Data = "unit";
        view.Size = 4;
        xrtValueObjectSetNew(result, view, xrtValueInt(f[6]));
        view.Data = "function";
        view.Size = 8;
        xrtValueObjectSetNew(result, view, xrtValueInt(function));
    }
    if (function & 0x80) {
        xstrview view;
        view.Data = "exception";
        view.Size = 9;
        xrtValueObjectSetNew(result, view, xrtValueInt(1));
        view.Data = "exceptionCode";
        view.Size = 13;
        xrtValueObjectSetNew(result, view, xrtValueInt(len > 8 ? f[8] : 0));
        return result;
    }
    {
        xstrview view;
        view.Data = "exception";
        view.Size = 9;
        xrtValueObjectSetNew(result, view, xrtValueInt(0));
    }
    if (function == 0x01 || function == 0x02 || function == 0x03 ||
        function == 0x04) {
        xvalue *values = xrtValueArray();
        int byteCount = len > 8 ? f[8] : 0;
        int i;
        xstrview view;
        if (values == NULL || 9 + byteCount > len) {
            if (values != NULL) xrtValueRelease(values);
            xm_error("read response data is truncated");
            return xm_null();
        }
        if ((function == 0x03 || function == 0x04) && byteCount % 2 != 0) {
            xrtValueRelease(values);
            xm_error("register byte count must be even");
            return xm_null();
        }
        if (function == 0x03 || function == 0x04) {
            for (i = 0; i + 1 < byteCount; i += 2) {
                xrtValueArrayAppendNew(values,
                    xrtValueInt((f[9 + i] << 8) | f[9 + i + 1]));
            }
        } else {
            for (i = 0; i < byteCount; i++) {
                xrtValueArrayAppendNew(values, xrtValueInt(f[9 + i]));
            }
        }
        view.Data = "values";
        view.Size = 6;
        xrtValueObjectSetNew(result, view, values);
        return result;
    }
    if (function == 0x05 || function == 0x06 || function == 0x10) {
        xstrview view;
        view.Data = "address";
        view.Size = 7;
        xrtValueObjectSetNew(result, view,
            xrtValueInt(len > 10 ? ((f[8] << 8) | f[9]) : 0));
        view.Data = "value";
        view.Size = 5;
        xrtValueObjectSetNew(result, view,
            xrtValueInt(len > 11 ? ((f[10] << 8) | f[11]) : 0));
        return result;
    }
    xm_error("unsupported function code in response");
    return xm_null();
}

/* Convert a hex frame into a fresh buffer; the xlang side wraps the
   returned pointer in stdlib Buffer(point) and owns it from then on. */
void *xm_hex_to_frame(const char *hex) {
    unsigned char bytes[263];
    int len = xm_hex_to_bytes(hex, bytes, sizeof(bytes));
    xbuffer *buffer;
    int i;
    if (len < 0) {
        xm_error("invalid hex frame");
        return NULL;
    }
    buffer = xrtBufferCreate();
    if (buffer == NULL) return NULL;
    for (i = 0; i < len; i++) {
        if (!xrtBufferAppendByte(buffer, bytes[i])) {
            xrtBufferDestroy(buffer);
            return NULL;
        }
    }
    return buffer;
}

/* Convert raw received memory [data, data+size) to hex. */
const char *xm_frame_to_hex(void *data, long long size) {
    static char hex[528];
    hex[0] = '\0';
    if (data == NULL || size < 0 || size > 263) {
        xm_error("frameToHex size must be 0-263");
        return hex;
    }
    xm_bytes_to_hex((const unsigned char *)data, (size_t)size, hex);
    return hex;
}
