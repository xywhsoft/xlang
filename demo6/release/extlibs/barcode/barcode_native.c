/*
 * barcode_native.c - EAN-13 and UPC-A barcode generation.
 *
 * Layout follows the standard: guard 101, six left digits encoded with
 * the L (odd) or G (even) pattern chosen by the first digit's parity
 * table, middle guard 01010, six right digits in R patterns, and a
 * trailing guard 101. R is the complement of L; G is R reversed.
 * UPC-A is emitted as EAN-13 with a leading zero.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XB_DOMAIN "xlang.extlib.barcode"

static void xb_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XB_DOMAIN, 0, message);
}

/* Seven-module patterns for digits 0-9 in L coding. */
static const char *xb_l[10] = {
    "0001101", "0011001", "0010011", "0111101", "0100011",
    "0110001", "0101111", "0111011", "0110111", "0001011"
};

/* Parity table: for first digit d, the L/G pattern of left digits 2-7. */
static const char *xb_parity[10] = {
    "LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
    "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"
};

static int xb_digit(char c) {
    if (c < '0' || c > '9') return -1;
    return c - '0';
}

/* Standard EAN check digit over the first n digits. */
static int xb_check_digit(const char *digits, int n) {
    int sum = 0;
    int i;
    for (i = 0; i < n; i++) {
        int d = xb_digit(digits[i]);
        if (d < 0) return -1;
        sum += d * ((i & 1) == 0 ? 1 : 3);
    }
    return (10 - sum % 10) % 10;
}

/* Build 95 modules for 13 EAN digits; returns module count or 0. */
static int xb_build_modules(const char *d13, char *out /*95+1*/) {
    int first = xb_digit(d13[0]);
    const char *parity;
    int i;
    int pos = 0;
    if (first < 0) return 0;
    parity = xb_parity[first];
    memcpy(out + pos, "101", 3);
    pos += 3;
    for (i = 1; i <= 6; i++) {
        int d = xb_digit(d13[i]);
        int k;
        if (d < 0) return 0;
        if (parity[i - 1] == 'L') {
            memcpy(out + pos, xb_l[d], 7);
        } else {
            /* G = reverse of R = reverse of complement of L */
            for (k = 0; k < 7; k++) {
                out[pos + k] = xb_l[d][6 - k] == '1' ? '0' : '1';
            }
        }
        pos += 7;
    }
    memcpy(out + pos, "01010", 5);
    pos += 5;
    for (i = 7; i <= 12; i++) {
        int d = xb_digit(d13[i]);
        int k;
        if (d < 0) return 0;
        for (k = 0; k < 7; k++) {
            out[pos + k] = xb_l[d][k] == '1' ? '0' : '1';
        }
        pos += 7;
    }
    memcpy(out + pos, "101", 3);
    pos += 3;
    out[pos] = '\0';
    return pos;
}

/* Normalize EAN-13 input: 12 digits -> append check; 13 -> validate. */
static int xb_normalize_ean13(const char *input, char *out13) {
    size_t n = input != NULL ? strlen(input) : 0;
    if (n == 12) {
        int check = xb_check_digit(input, 12);
        int i;
        if (check < 0) return 0;
        for (i = 0; i < 12; i++) out13[i] = input[i];
        out13[12] = (char)('0' + check);
        out13[13] = '\0';
        return 1;
    }
    if (n == 13) {
        int check = xb_check_digit(input, 12);
        if (check < 0) return 0;
        if (input[12] - '0' != check) return 0;
        memcpy(out13, input, 13);
        out13[13] = '\0';
        return 1;
    }
    return 0;
}

xvalue *xb_ean13(const char *digits) {
    char d13[16];
    char modules[128];
    xvalue *result;
    int count;
    int i;
    if (!xb_normalize_ean13(digits, d13)) {
        xb_error("ean13 needs 12 digits (checksum added) or 13 valid digits");
        return xrtValueNull();
    }
    count = xb_build_modules(d13, modules);
    if (count == 0) return xrtValueNull();
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    for (i = 0; i < count; i++) {
        if (!xrtValueArrayAppendNew(result,
                xrtValueInt(modules[i] == '1' ? 1 : 0))) {
            return xrtValueNull();
        }
    }
    return result;
}

xvalue *xb_upc_a(const char *digits) {
    char withZero[20];
    char d13[16];
    char modules[128];
    xvalue *result;
    int count;
    int i;
    size_t n = digits != NULL ? strlen(digits) : 0;
    if (n == 11) {
        char twelve[16];
        int check;
        int k;
        /* UPC-A check digit uses the same weights shifted by the 0 */
        twelve[0] = '0';
        memcpy(twelve + 1, digits, 11);
        twelve[12] = '\0';
        check = xb_check_digit(twelve, 12);
        if (check < 0) return xrtValueNull();
        for (k = 0; k < 11; k++) withZero[k + 1] = digits[k];
        withZero[0] = '0';
        withZero[11] = (char)('0' + check);
        withZero[12] = '\0';
    } else if (n == 12) {
        memcpy(withZero + 1, digits, 12);
        withZero[0] = '0';
        withZero[13] = '\0';
    } else {
        xb_error("upcA needs 11 digits (checksum added) or 12 valid digits");
        return xrtValueNull();
    }
    if (!xb_normalize_ean13(withZero, d13)) {
        xb_error("upcA checksum mismatch");
        return xrtValueNull();
    }
    count = xb_build_modules(d13, modules);
    if (count == 0) return xrtValueNull();
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    for (i = 0; i < count; i++) {
        if (!xrtValueArrayAppendNew(result,
                xrtValueInt(modules[i] == '1' ? 1 : 0))) {
            return xrtValueNull();
        }
    }
    return result;
}

long long xb_checksum_ean(const char *digits) {
    size_t n = digits != NULL ? strlen(digits) : 0;
    if (n == 0) return -1;
    return xb_check_digit(digits, (int)n);
}

long long xb_validate_ean(const char *code) {
    size_t n = code != NULL ? strlen(code) : 0;
    char d13[16];
    if (n != 13) return 0;
    if (!xb_normalize_ean13(code, d13)) return 0;
    return 1;
}

const char *xb_svg(const char *digits, int isUpcA, long long height,
    long long scale) {
    static char ring[2][32768];
    static int next;
    char d13[16];
    char modules[128];
    int count;
    int i;
    char header[512];
    size_t pos;
    next = (next + 1) % 2;
    ring[next][0] = '\0';
    if (height < 1) height = 60;
    if (scale < 1) scale = 3;
    if (isUpcA) {
        char withZero[20];
        size_t n = strlen(digits);
        if (n == 11) {
            char twelve[16];
            int check;
            memcpy(twelve + 1, digits, 11);
            twelve[0] = '0';
            twelve[12] = '\0';
            check = xb_check_digit(twelve, 12);
            if (check < 0) return ring[next];
            withZero[0] = '0';
            memcpy(withZero + 1, digits, 11);
            withZero[12] = (char)('0' + check);
            withZero[13] = '\0';
        } else if (n == 12) {
            memcpy(withZero + 1, digits, 12);
            withZero[0] = '0';
            withZero[13] = '\0';
        } else {
            return ring[next];
        }
        if (!xb_normalize_ean13(withZero, d13)) return ring[next];
    } else {
        if (!xb_normalize_ean13(digits, d13)) return ring[next];
    }
    count = xb_build_modules(d13, modules);
    if (count == 0) return ring[next];
    {
        long long dim = (long long)count * scale;
        snprintf(header, sizeof(header),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "viewBox=\"0 0 %lld %lld\">\n"
            "  <rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n"
            "  <path fill=\"#000000\" d=\"",
            dim, height);
        strcpy(ring[next], header);
    }
    pos = strlen(ring[next]);
    for (i = 0; i < count; i++) {
        if (modules[i] == '1') {
            char cell[64];
            snprintf(cell, sizeof(cell), "M%lld,0h%lldv%lldh-%lldz",
                (long long)i * scale, scale, height, scale);
            if (pos + strlen(cell) + 1 < sizeof(ring[0]) - 8) {
                strcpy(ring[next] + pos, cell);
                pos += strlen(cell);
            }
        }
    }
    strcpy(ring[next] + pos, "\"/>\n</svg>\n");
    return ring[next];
}

const char *xb_ascii(const char *digits, int isUpcA) {
    static char ring[2][512];
    static int next;
    char d13[16];
    char modules[128];
    int count;
    char withZero[20];
    int i;
    size_t pos = 0;
    next = (next + 1) % 2;
    ring[next][0] = '\0';
    if (isUpcA) {
        size_t n = strlen(digits);
        if (n != 12) return ring[next];
        memcpy(withZero + 1, digits, 12);
        withZero[0] = '0';
        withZero[13] = '\0';
        if (!xb_normalize_ean13(withZero, d13)) return ring[next];
    } else {
        if (!xb_normalize_ean13(digits, d13)) return ring[next];
    }
    count = xb_build_modules(d13, modules);
    if (count == 0) return ring[next];
    for (i = 0; i < count; i++) {
        if (pos + 2 < sizeof(ring[0])) {
            ring[next][pos++] = modules[i] == '1' ? '#' : ' ';
        }
    }
    ring[next][pos] = '\0';
    return ring[next];
}
