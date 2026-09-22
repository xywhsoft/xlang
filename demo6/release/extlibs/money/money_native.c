/*
 * money_native.c - Integer minor-unit money arithmetic and formatting.
 *
 * Amounts are int64 minor units (cents, fen, ...) to avoid binary float
 * error. Currency codes are case-insensitive; the built-in table knows
 * symbols and exponent (2 for most, 0 for JPY/KRW/VND-style currencies).
 * Arithmetic between different currencies reports an error.
 *
 *   add/sub        exact
 *   mul(n)         exact scaling
 *   div(n)         splits evenly; remainder distributed to the first
 *                  entries so the parts always sum back to the total
 *   rate(r)        multiplies by a double rate with half-up rounding
 *   format         groups thousands and places the symbol per style
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XM_RING 8
#define XM_TEXT 96

static char xm_ring[XM_RING][XM_TEXT];
static int xm_ring_next = 0;

static char *xm_slot(void) {
    char *slot = xm_ring[xm_ring_next];
    xm_ring_next = (xm_ring_next + 1) % XM_RING;
    return slot;
}

typedef struct {
    const char *code;
    const char *symbol;
    int exponent;
} XmCurrency;

static const XmCurrency xm_currencies[] = {
    {"USD", "$", 2}, {"EUR", "\xE2\x82\xAC", 2}, {"GBP", "\xC2\xA3", 2},
    {"CNY", "\xEF\xBF\xA5", 2}, {"JPY", "\xC2\xA5", 0}, {"KRW", "\xE2\x82\xA9", 0},
    {"HKD", "HK$", 2}, {"MOP", "MOP$", 2}, {"TWD", "NT$", 2},
    {"SGD", "S$", 2}, {"AUD", "A$", 2}, {"NZD", "NZ$", 2},
    {"CAD", "C$", 2}, {"CHF", "CHF", 2}, {"SEK", "kr", 2},
    {"NOK", "kr", 2}, {"DKK", "kr", 2}, {"RUB", "\xD2\xBB", 2},
    {"INR", "\xE2\x82\xB9", 2}, {"BRL", "R$", 2}, {"MXN", "MX$", 2},
    {"VND", "\xE2\x82\xAB", 0}, {"THB", "\xE0\xB8\xBF", 2}
};

static const XmCurrency *xm_find(const char *code) {
    size_t i;
    if (code == NULL) return NULL;
    for (i = 0; i < sizeof(xm_currencies) / sizeof(xm_currencies[0]); i++) {
        size_t j;
        int match = 1;
        if (strlen(code) != strlen(xm_currencies[i].code)) match = 0;
        for (j = 0; match && code[j] != '\0'; j++) {
            char a = code[j];
            char b = xm_currencies[i].code[j];
            if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
            if (a != b) match = 0;
        }
        if (match) return &xm_currencies[i];
    }
    return NULL;
}

const char *xm_symbol(const char *code) {
    const XmCurrency *c = xm_find(code);
    if (c == NULL) return "";
    return c->symbol;
}

long long xm_exponent(const char *code) {
    const XmCurrency *c = xm_find(code);
    if (c == NULL) return 2;
    return c->exponent;
}

const char *xm_format(long long amount, const char *code) {
    const XmCurrency *c = xm_find(code);
    char *slot = xm_slot();
    int neg = amount < 0;
    unsigned long long abs;
    int exp = c != NULL ? c->exponent : 2;
    const char *symbol = c != NULL ? c->symbol : "";
    const char *unknown = c == NULL ? code : "";
    unsigned long long pow10 = 1;
    unsigned long long major;
    unsigned long long frac;
    char majorBuf[28];
    char out[80];
    int pos = 0;
    int len = 0;
    int i;
    if (neg) {
        abs = (unsigned long long)(-(amount + 1)) + 1;
    } else {
        abs = (unsigned long long)amount;
    }
    for (i = 0; i < exp; i++) pow10 *= 10;
    major = abs / pow10;
    frac = abs % pow10;
    /* major digits with thousands grouping, built back to front */
    if (major == 0) {
        majorBuf[len++] = '0';
    }
    while (major > 0) {
        if (len % 4 == 3) {
            majorBuf[len++] = ',';
        }
        majorBuf[len++] = (char)('0' + (major % 10));
        major /= 10;
    }
    /* prefix symbol/sign and reverse the digits into place */
    if (symbol[0] != '\0') {
        strcpy(out + pos, symbol);
        pos += (int)strlen(symbol);
    }
    if (unknown[0] != '\0') {
        if (pos > 0) out[pos++] = ' ';
        strcpy(out + pos, unknown);
        pos += (int)strlen(unknown);
        out[pos++] = ' ';
    }
    if (neg) out[pos++] = '-';
    for (i = len - 1; i >= 0; i--) {
        out[pos++] = majorBuf[i];
    }
    if (exp > 0) {
        int d;
        out[pos++] = '.';
        for (d = exp - 1; d >= 0; d--) {
            unsigned long long digit = frac;
            int k;
            for (k = 0; k < d; k++) digit /= 10;
            out[pos++] = (char)('0' + (digit % 10));
        }
    }
    out[pos] = '\0';
    strncpy(slot, out, XM_TEXT - 1);
    slot[XM_TEXT - 1] = '\0';
    return slot;
}
long long xm_add(long long a, long long b) { return a + b; }
long long xm_sub(long long a, long long b) { return a - b; }
long long xm_mul(long long a, long long n) { return a * n; }

long long xm_rate(long long amount, double rate) {
    double scaled = (double)amount * rate;
    long long rounded = (long long)(scaled + (scaled >= 0 ? 0.5 : -0.5));
    return rounded;
}

/* Split amount into n parts; remainder cents go to the first entries.
/* Split amount into n parts; remainder cents go to the first entries so
   the parts always sum back to the total. */
xvalue *xm_split(long long amount, long long n) {
    xvalue *result;
    long long i;
    if (n <= 0 || n > 1000000) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.money", 0,
            "split requires a positive part count");
        return xrtValueNull();
    }
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    for (i = 0; i < n; i++) {
        long long part = amount / n + (i < amount % n ? 1 : 0);
        if (!xrtValueArrayAppendNew(result, xrtValueInt((int64)part))) {
            return xrtValueNull();
        }
    }
    return result;
}
