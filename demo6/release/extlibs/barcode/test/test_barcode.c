#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "barcode_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "barcode C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int same_modules(const xvalue *left, const xvalue *right) {
    int i;
    if (left == NULL || right == NULL || xrtValueCount(left) != 95 || xrtValueCount(right) != 95) return 0;
    for (i = 0; i < 95; ++i) {
        int64 a, b;
        if (!xrtValueGetInt(xrtValueArrayAt(left, i), &a) ||
            !xrtValueGetInt(xrtValueArrayAt(right, i), &b) || a != b) return 0;
    }
    return 1;
}

static int matches_bits(const xvalue *modules, const char *bits) {
    int i;
    if (modules == NULL || strlen(bits) != 95 || xrtValueCount(modules) != 95) return 0;
    for (i = 0; i < 95; ++i) {
        int64 bit;
        if (!xrtValueGetInt(xrtValueArrayAt(modules, i), &bit) ||
            bit != bits[i] - '0') return 0;
    }
    return 1;
}

static int test_codes(void) {
    /* 独立位串防止 UPC-A/EAN-13 两条路径以相同错误结果互相通过。 */
    static const char ean_bits[] =
        "10100011010100111010111101111010001001011001101010"
        "100001010000101000010111010010000101100110101";
    static const char upc_bits[] =
        "10100110010010011011110101000110110001010111101010"
        "100010010010001110100111001011001101101100101";
    xvalue *upc_short = xb_upc_a("12345678901", 11);
    xvalue *upc_full = xb_upc_a("123456789012", 12);
    xvalue *ean = xb_ean13("0123456789012", 13);
    xvalue *ean_nonzero = xb_ean13("4006381333931", 13);
    xvalue *wrong;
    CHECK(xb_checksum_ean("400638133393", 12) == 1);
    CHECK(xb_checksum_ean("4006381333931", 13) == -1);
    CHECK(xb_validate_ean("4006381333931", 13));
    CHECK(!xb_validate_ean("4006381333932", 13));
    CHECK(!xb_validate_ean("40063813339\0", 13));
    CHECK(same_modules(upc_short, ean) && same_modules(upc_full, ean));
    CHECK(matches_bits(ean_nonzero, ean_bits));
    CHECK(matches_bits(upc_short, upc_bits));
    wrong = xb_upc_a("123456789019", 12);
    CHECK(wrong != NULL && xrtValueType(wrong) == XVALUE_NULL);
    xrtValueRelease(wrong);
    wrong = xb_ean13("400638133393\0", 13);
    CHECK(wrong != NULL && xrtValueType(wrong) == XVALUE_NULL);
    xrtValueRelease(wrong);
    wrong = xb_ean13("4006381333931extra", 18);
    CHECK(wrong != NULL && xrtValueType(wrong) == XVALUE_NULL);
    xrtValueRelease(wrong);
    xrtValueRelease(upc_short);
    xrtValueRelease(upc_full);
    xrtValueRelease(ean);
    xrtValueRelease(ean_nonzero);
    return 0;
}

static int test_ean_parity(void) {
    /* EAN-13 首位选择左六位的 L/G 模式；掩码高位对应最左位。 */
    static const unsigned char g_mask[10] = {
        0x00, 0x0B, 0x0D, 0x0E, 0x13, 0x19, 0x1C, 0x15, 0x16, 0x1A
    };
    static const char left[] = "0001101";
    static const char right[] = "0100111";
    char digits[] = "000000000000";
    int first, slot, bit;
    for (first = 0; first < 10; ++first) {
        xvalue *modules;
        digits[0] = (char)('0' + first);
        modules = xb_ean13(digits, 12);
        CHECK(modules != NULL && xrtValueCount(modules) == 95);
        for (slot = 0; slot < 6; ++slot) {
            const char *expected = (g_mask[first] & (1u << (5 - slot))) ? right : left;
            for (bit = 0; bit < 7; ++bit) {
                int64 actual;
                CHECK(xrtValueGetInt(xrtValueArrayAt(modules, 3 + slot * 7 + bit), &actual));
                CHECK(actual == expected[bit] - '0');
            }
        }
        xrtValueRelease(modules);
    }
    return 0;
}

static int reference_check_digit(const char data[12]) {
    int ordinary = 0, triple = 0;
    for (int i = 0; i < 12; ++i) {
        if (i & 1) triple += data[i] - '0';
        else ordinary += data[i] - '0';
    }
    return (10 - (ordinary + 3 * triple) % 10) % 10;
}

static int test_checksum_matrix(void) {
    char code[14];
    unsigned state = 0x6B8B4567u;
    unsigned seen = 0;
    for (int sample = 0; sample < 256; ++sample) {
        xvalue *short_code, *full_code;
        if (sample < 10) {
            /* 固定首组确保 0..9 每种校验位都进入测试。 */
            memset(code, '0', 12);
            code[11] = (char)('0' + sample);
        } else {
            for (int i = 0; i < 12; ++i) {
                state = state * 1664525u + 1013904223u;
                code[i] = (char)('0' + state % 10u);
            }
        }
        int expected = reference_check_digit(code);
        code[12] = (char)('0' + expected);
        code[13] = '\0';
        seen |= 1u << expected;
        CHECK(xb_checksum_ean(code, 12) == expected);
        CHECK(xb_validate_ean(code, 13));
        code[12] = (char)('0' + (expected + 1) % 10);
        CHECK(!xb_validate_ean(code, 13));
        code[12] = (char)('0' + expected);
        short_code = xb_ean13(code, 12);
        full_code = xb_ean13(code, 13);
        CHECK(same_modules(short_code, full_code));
        if (sample < 10) {
            xvalue *upc_short = xb_upc_a(code + 1, 11);
            xvalue *upc_full = xb_upc_a(code + 1, 12);
            CHECK(same_modules(short_code, upc_short));
            CHECK(same_modules(short_code, upc_full));
            xrtValueRelease(upc_short);
            xrtValueRelease(upc_full);
        }
        xrtValueRelease(short_code);
        xrtValueRelease(full_code);
    }
    CHECK(seen == 0x3FFu);
    return 0;
}

static int test_rendering(void) {
    void *ascii = xb_ascii("4006381333931", 13, false);
    void *upc_ascii = xb_ascii("12345678901", 11, true);
    void *svg = xb_svg("4006381333931", 13, false, 60, 3);
    void *upc_svg = xb_svg("12345678901", 11, true, 60, 3);
    void *large = xb_svg("4006381333931", 13, false, 1000000, 1000000);
    CHECK(ascii && upc_ascii && svg && upc_svg && large);
    CHECK(strlen((const char *)ascii) == 95);
    CHECK(strlen((const char *)upc_ascii) == 95);
    /* GS1 静区：EAN-13 左 11X/右 7X，UPC-A 左右各 9X。 */
    CHECK(strstr((const char *)svg, "viewBox=\"0 0 339 60\"") != NULL);
    CHECK(strstr((const char *)svg, "d=\"M33,0h3v60h-3z") != NULL);
    CHECK(strstr((const char *)svg, "M315,0h3v60h-3z") != NULL);
    CHECK(strstr((const char *)upc_svg, "viewBox=\"0 0 339 60\"") != NULL);
    CHECK(strstr((const char *)upc_svg, "d=\"M27,0h3v60h-3z") != NULL);
    CHECK(strstr((const char *)upc_svg, "M309,0h3v60h-3z") != NULL);
    CHECK(strstr((const char *)large, "viewBox=\"0 0 113000000 1000000\"") != NULL);
    CHECK(strstr((const char *)large, "</svg>") != NULL);
    CHECK(strlen((const char *)ascii) == 95 && strstr((const char *)svg, "</svg>") != NULL);
    CHECK(xb_svg("4006381333931", 13, false, 0, 3) == NULL);
    CHECK(xb_ascii("bad", 3, false) == NULL);
    xrtFree(ascii);
    xrtFree(upc_ascii);
    xrtFree(svg);
    xrtFree(upc_svg);
    xrtFree(large);
    return 0;
}

static int test_allocation_errors(void) {
    int api;
    for (api = 0; api < 4; ++api) {
        unsigned failure;
        for (failure = 0; failure < 256; ++failure) {
            xvalue *modules = NULL;
            void *text = NULL;
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            if (api == 0) modules = xb_ean13("4006381333931", 13);
            if (api == 1) modules = xb_upc_a("12345678901", 11);
            if (api == 2) text = xb_svg("4006381333931", 13, false, 60, 3);
            if (api == 3) text = xb_ascii("4006381333931", 13, false);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK((modules == NULL || xrtValueType(modules) == XVALUE_NULL) &&
                    text == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                if (api < 2) CHECK(modules != NULL && xrtValueCount(modules) == 95);
                if (api == 2) CHECK(text != NULL && strstr((const char *)text, "</svg>") != NULL);
                if (api == 3) CHECK(text != NULL && strlen((const char *)text) == 95);
            }
            xrtValueRelease(modules);
            xrtFree(text);
            xrtClearError();
            if (!triggered) break;
        }
        CHECK(failure > 0 && failure < 256);
    }
    return 0;
}

static int test_error_recovery(void) {
    xvalue *modules;
    void *text;
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    CHECK(xb_checksum_ean("400638133393", 12) == 1 && xrtGetError() == NULL);
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    CHECK(xb_validate_ean("4006381333931", 13) && xrtGetError() == NULL);
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    modules = xb_ean13("4006381333931", 13);
    CHECK(modules != NULL && xrtGetError() == NULL && xrtValueCount(modules) == 95);
    xrtValueRelease(modules);
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    modules = xb_upc_a("12345678901", 11);
    CHECK(modules != NULL && xrtGetError() == NULL && xrtValueCount(modules) == 95);
    xrtValueRelease(modules);
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    text = xb_svg("4006381333931", 13, false, 60, 3);
    CHECK(text != NULL && xrtGetError() == NULL);
    xrtFree(text);
    CHECK(xb_svg("bad", 3, false, 60, 3) == NULL && xrtGetError() != NULL);
    text = xb_ascii("4006381333931", 13, false);
    CHECK(text != NULL && xrtGetError() == NULL);
    xrtFree(text);
    return 0;
}

int main(void) {
    if (test_codes() || test_ean_parity() || test_checksum_matrix() ||
        test_rendering() || test_allocation_errors() ||
        test_error_recovery()) return 1;
    puts("barcode C: PASS");
    return 0;
}
