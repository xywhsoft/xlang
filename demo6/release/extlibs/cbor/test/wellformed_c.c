#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_BUFFER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../cbor_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef __TINYC__
#include "../cbor_native.c"
#endif

static unsigned checks, failures;

static void check(bool condition, const char *where) {
    ++checks;
    if (!condition) {
        if (failures < 12) fprintf(stderr, "CBOR %s failed at check %u\n", where, checks);
        ++failures;
    }
}

static void balanced(xmemdebugsnapshot before) {
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount,
        "allocation balance");
}

static void reject(const unsigned char *wire, size_t size, xerrkind kind) {
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    xvalue *value = xcbor_decode(wire, size);
    check(value == NULL && xrtErrorKind(xrtGetError()) == kind, "value rejection/category");
    xrtValueRelease(value);
    balanced(before);
    xbuffer *raw = xcbor_decode_bytes(wire, size);
    check(raw == NULL && xrtErrorKind(xrtGetError()) == kind, "bytes rejection/category");
    xrtBufferDestroy(raw);
    balanced(before);
    /* 失败后马上恢复；合法null不能与失败的C NULL混淆。 */
    value = xcbor_decode("\xf6", 1);
    check(value != NULL && xrtValueType(value) == XVALUE_NULL && !xcbor_failed(), "recovery");
    xrtValueRelease(value);
    balanced(before);
}

/* RFC8949 F.1：同一个非法头在根、定长/不定长数组和映射值位置都要拒绝。 */
static void positions(const unsigned char *token, size_t size, xerrkind kind) {
    static const unsigned char prefixes[][3] = {{0}, {0x81}, {0x9f}, {0xa1,0x61,'x'}, {0xbf,0x61,'x'}};
    static const size_t sizes[] = {0, 1, 1, 3, 3};
    unsigned char wire[16];
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        memcpy(wire, prefixes[i], sizes[i]);
        memcpy(wire+sizes[i], token, size);
        size_t total = sizes[i]+size;
        if (i == 2 || i == 4) wire[total++] = 0xff;
        reject(wire, total, kind);
    }
}

static void syntax_cases(void) {
    for (unsigned major = 0; major < 8; ++major) {
        for (unsigned info = 28; info <= 30; ++info) {
            unsigned char token = (unsigned char)((major<<5)|info);
            positions(&token, 1, XERR_VALUE);
        }
    }
    for (unsigned major = 0; major <= 6; ++major) {
        if (major != 0 && major != 1 && major != 6) continue;
        unsigned char token = (unsigned char)((major<<5)|31);
        positions(&token, 1, XERR_VALUE);
    }
    for (unsigned value = 0; value < 32; ++value) {
        unsigned char token[] = {0xf8, (unsigned char)value};
        positions(token, sizeof(token), XERR_VALUE);
    }
    /* 未建模但语法合法的simple值仍报UNSUPPORTED，不能一律当坏格式。 */
    for (unsigned value = 0; value < 20; ++value) {
        unsigned char token = (unsigned char)(0xe0|value);
        positions(&token, 1, XERR_UNSUPPORTED);
    }
    for (unsigned value = 32; value <= 255; value += 223) {
        unsigned char token[] = {0xf8, (unsigned char)value};
        positions(token, sizeof(token), XERR_UNSUPPORTED);
    }
    {
        unsigned char token = 0xf7;
        positions(&token, 1, XERR_UNSUPPORTED);
    }
    static const struct { unsigned char wire[8]; size_t size; } malformed[] = {
        {{0x5f,0x00,0xff},3}, {{0x5f,0x21,0xff},3}, {{0x5f,0x61,0x00,0xff},4},
        {{0x5f,0x80,0xff},3}, {{0x5f,0xa0,0xff},3}, {{0x5f,0xc0,0x00,0xff},4},
        {{0x5f,0xe0,0xff},3}, {{0x7f,0x41,0x00,0xff},4},
        {{0x5f,0x5f,0x41,0x00,0xff,0xff},6}, {{0x7f,0x7f,0x61,0x00,0xff,0xff},6},
        {{0xff},1}, {{0x81,0xff},2}, {{0xa1,0x61,'x',0xff},4},
        {{0xbf,0x61,'x',0xff},4}
    };
    for (size_t i = 0; i < sizeof(malformed)/sizeof(malformed[0]); ++i)
        reject(malformed[i].wire, malformed[i].size, XERR_VALUE);
}

static void chunks_budget(void) {
    const size_t limit = 1000000;
    unsigned char *wire = malloc(limit+8);
    if (!wire) { check(false, "fixture allocation"); return; }
    for (unsigned shape = 0; shape < 3; ++shape) {
        size_t prefix = shape == 0 ? 0 : shape == 1 ? 1 : 3;
        for (unsigned text = 0; text < 2; ++text) {
            for (unsigned extra = 0; extra < 3; ++extra) {
                size_t count = limit-1-(shape != 0)+extra;
                xmemdebugsnapshot before;
                if (shape == 1) wire[0] = 0x81;
                if (shape == 2) { wire[0] = 0xa1; wire[1] = 0x61; wire[2] = 'x'; }
                wire[prefix] = text ? 0x7f : 0x5f;
                memset(wire+prefix+1, text ? 0x60 : 0x40, count);
                wire[prefix+count+1] = 0xff;
                size_t size = prefix+count+2;
                xrtClearError(); xrtMemDebugSnapshot(&before);
                xvalue *value = xcbor_decode(wire, size);
                if (extra == 2) {
                    check(value == NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE, "shared chunk limit");
                } else {
                    const xvalue *item = !value || shape == 0 ? value : shape == 1 ?
                        xrtValueArrayGet(value, 0) : xrtValueObjectGet(value, XRT_STR_LITERAL("x"));
                    xstrview string = {0}; xbytesview raw = {0};
                    check(item && !xcbor_failed() && (text ?
                        (xrtValueGetString(item, &string) && string.Size == 0) :
                        (xrtValueGetBytes(item, &raw) && raw.Size == 0)), "chunk limit success");
                }
                xrtValueRelease(value); balanced(before);
                if (!shape && !text) {
                    xbuffer *raw = xcbor_decode_bytes(wire, size);
                    check(extra == 2 ? (!raw && xrtErrorKind(xrtGetError()) == XERR_RANGE) :
                        (raw && !xcbor_failed() && raw->Size == 0), "direct bytes chunk limit");
                    xrtBufferDestroy(raw); balanced(before);
                }
            }
        }
    }
    free(wire);
}

/* 单独输出全部binary16解码结果，交给Python标准库struct独立核对。 */
static int half_records(void) {
    for (unsigned bits = 0; bits <= 65535; ++bits) {
        const unsigned char wire[] = {0xf9, (unsigned char)(bits>>8), (unsigned char)bits};
        xvalue *value = xcbor_decode(wire, sizeof(wire));
        double actual;
        uint64_t representation;
        if (!value || xcbor_failed() || !xrtValueGetFloat(value, &actual)) {
            xrtValueRelease(value); return 2;
        }
        memcpy(&representation, &actual, sizeof(representation));
        printf("%04x\t%016" PRIx64 "\n", bits, representation);
        xrtValueRelease(value);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--half-records") == 0) return half_records();
    if (argc != 1) return 2;
    syntax_cases();
    chunks_budget();
    /* 非最短的整型头仍是合法CBOR；不能和f8的保留二字节编码混淆。 */
    static const unsigned char zero[][9] = {{0}, {0x18,0}, {0x19,0,0}, {0x1a,0,0,0,0}, {0x1b,0,0,0,0,0,0,0,0}};
    static const size_t sizes[] = {1,2,3,5,9};
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        xvalue *value = xcbor_decode(zero[i], sizes[i]);
        int64 number = 1;
        check(value && !xcbor_failed() && xrtValueGetInt(value, &number) && number == 0, "nonpreferred integer");
        xrtValueRelease(value);
    }
    printf("cbor well-formedness: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
