/* EAN-13 / UPC-A 共用 95 模块编码；输入统一使用显式字节长度。 */
#include "include/barcode_native.h"

#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"

#define XB_DOMAIN "xlang.extlib.barcode"

static const char *xb_l[10] = {
    "0001101", "0011001", "0010011", "0111101", "0100011",
    "0110001", "0101111", "0111011", "0110111", "0001011"
};

static const char *xb_parity[10] = {
    "LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
    "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"
};

static int xb_fail(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XB_DOMAIN, 0, message);
    return 0;
}

static int xb_digit(char digit) {
    return digit >= '0' && digit <= '9' ? digit - '0' : -1;
}

/* EAN-13 前十二位的权重为 1,3,1,3,...；UPC-A 在前面补 0 后相同。 */
static int xb_check_digit(const char *digits) {
    int sum = 0, i;
    for (i = 0; i < 12; ++i) {
        int digit = xb_digit(digits[i]);
        if (digit < 0) return -1;
        sum += digit * (i % 2 ? 3 : 1);
    }
    return (10 - sum % 10) % 10;
}

/* 统一成含校验位的 13 位 EAN；UPC 输入为 11/12 位，不覆盖数据位。 */
static int xb_normalize(const char *digits, size_t size, bool upc, char out[14]) {
    size_t i, offset = upc ? 1 : 0;
    int check;
    if (digits == NULL || (upc ? (size != 11 && size != 12) : (size != 12 && size != 13)))
        return xb_fail(upc ? "UPC-A requires 11 or 12 digits" : "EAN-13 requires 12 or 13 digits");
    for (i = 0; i < size; ++i) {
        if (xb_digit(digits[i]) < 0) return xb_fail("barcode contains a non-digit byte");
    }
    if (upc) out[0] = '0';
    memcpy(out + offset, digits, size);
    check = xb_check_digit(out);
    if (size + offset == 12) out[12] = (char)('0' + check);
    else if (out[12] != '0' + check) return xb_fail("barcode checksum mismatch");
    out[13] = '\0';
    return 1;
}

static void xb_build_modules(const char digits[14], char modules[96]) {
    int i, k, pos = 0;
    const char *parity = xb_parity[digits[0] - '0'];
    memcpy(modules + pos, "101", 3);
    pos += 3;
    for (i = 1; i <= 6; ++i) {
        const char *pattern = xb_l[digits[i] - '0'];
        if (parity[i - 1] == 'L') memcpy(modules + pos, pattern, 7);
        else {
            for (k = 0; k < 7; ++k) modules[pos + k] = pattern[6 - k] == '1' ? '0' : '1';
        }
        pos += 7;
    }
    memcpy(modules + pos, "01010", 5);
    pos += 5;
    for (i = 7; i <= 12; ++i) {
        const char *pattern = xb_l[digits[i] - '0'];
        for (k = 0; k < 7; ++k) modules[pos + k] = pattern[k] == '1' ? '0' : '1';
        pos += 7;
    }
    memcpy(modules + pos, "101", 3);
    modules[95] = '\0';
}

static xvalue *xb_modules_value(const char modules[96]) {
    xvalue *array = xrtValueArray();
    int i;
    if (array == NULL) return xrtValueNull();
    for (i = 0; i < 95; ++i) {
        if (!xrtValueArrayAppendNew(array, xrtValueInt(modules[i] == '1' ? 1 : 0))) {
            xrtValueRelease(array);
            return xrtValueNull();
        }
    }
    return array;
}

/* ------------ 校验与模块数组 ------------ */

int64_t xb_checksum_ean(const char *digits, size_t size) {
    xrtClearError();
    if (digits == NULL || size != 12) return -1;
    return xb_check_digit(digits);
}

bool xb_validate_ean(const char *digits, size_t size) {
    size_t i;
    xrtClearError();
    if (digits == NULL || size != 13) return false;
    for (i = 0; i < 13; ++i) {
        if (xb_digit(digits[i]) < 0) return false;
    }
    return digits[12] - '0' == xb_check_digit(digits);
}

xvalue *xb_ean13(const char *digits, size_t size) {
    char normalized[14], modules[96];
    xrtClearError();
    if (!xb_normalize(digits, size, false, normalized)) return xrtValueNull();
    xb_build_modules(normalized, modules);
    return xb_modules_value(modules);
}

xvalue *xb_upc_a(const char *digits, size_t size) {
    char normalized[14], modules[96];
    xrtClearError();
    if (!xb_normalize(digits, size, true, normalized)) return xrtValueNull();
    xb_build_modules(normalized, modules);
    return xb_modules_value(modules);
}

/* ------------ 独立拥有的文本渲染结果 ------------ */

static int xb_append(xbuffer *output, const char *text) {
    return xrtBufferAppend(output, (xbytesview){(const uint8 *)text, strlen(text)});
}

void *xb_svg(const char *digits, size_t size, bool upc, int64_t height, int64_t scale) {
    char normalized[14], modules[96], part[256];
    xbuffer output;
    const int quiet_left = upc ? 9 : 11;
    const int quiet_right = upc ? 9 : 7;
    int i, count;
    xrtClearError();
    if (height < 1 || height > 1000000 || scale < 1 || scale > 1000000) {
        xb_fail("SVG height and scale must be 1..1000000");
        return NULL;
    }
    if (!xb_normalize(digits, size, upc, normalized)) return NULL;
    xb_build_modules(normalized, modules);
    if (!xrtBufferInit(&output)) return NULL;
    count = snprintf(part, sizeof(part),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %lld %lld\">\n"
        "  <rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n"
        "  <path fill=\"#000000\" d=\"",
        (long long)((95 + quiet_left + quiet_right) * scale), (long long)height);
    if (count < 0 || (size_t)count >= sizeof(part) || !xb_append(&output, part)) goto fail;
    for (i = 0; i < 95; ++i) {
        if (modules[i] != '1') continue;
        count = snprintf(part, sizeof(part), "M%lld,0h%lldv%lldh-%lldz",
            (long long)(i + quiet_left) * scale, (long long)scale,
            (long long)height, (long long)scale);
        if (count < 0 || (size_t)count >= sizeof(part) || !xb_append(&output, part)) goto fail;
    }
    if (!xb_append(&output, "\"/>\n</svg>\n") || !xrtBufferAppendByte(&output, 0)) goto fail;
    return xrtBufferTake(&output, NULL, NULL);
fail:
    xrtBufferUnit(&output);
    return NULL;
}

void *xb_ascii(const char *digits, size_t size, bool upc) {
    char normalized[14], modules[96], ascii[96];
    int i;
    xrtClearError();
    if (!xb_normalize(digits, size, upc, normalized)) return NULL;
    xb_build_modules(normalized, modules);
    for (i = 0; i < 95; ++i) ascii[i] = modules[i] == '1' ? '#' : ' ';
    ascii[95] = '\0';
    return xrtStrDup(ascii);
}
