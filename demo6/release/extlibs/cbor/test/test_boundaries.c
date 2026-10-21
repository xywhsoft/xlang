#define XRT_MODULE_ALL
#include "xrt.h"
#include "../cbor_native.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "CBOR boundary failed at %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int invalid(const unsigned char *wire, size_t size) {
    xvalue *value = xcbor_decode(wire, size);
    int failed = value == NULL && xcbor_failed();
    xrtValueRelease(value);
    xrtClearError();
    return failed;
}

static int oversized_input_errors(void) {
    size_t size = 64u * 1024u * 1024u + 1u;
    unsigned char* input = (unsigned char*)malloc(size);
    xvalue* value;
    xbuffer* buffer;
    char* hex;
    CHECK(input != NULL);
    memset(input, 0, size);
    value = xcbor_decode(input, size);
    CHECK(value == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    buffer = xcbor_decode_bytes(input, size);
    CHECK(buffer == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    buffer = xcbor_encode_bytes(input, size);
    CHECK(buffer == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    hex = xcbor_to_hex(input, size);
    CHECK(hex == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    free(input);
    return 0;
}

/* 固定输入扫描覆盖全部首字节及不同截断长度，避免只测手写样本。 */
static int short_input_scan(void) {
    static const size_t lengths[] = {1u, 2u, 3u, 5u, 9u, 17u, 33u, 65u};
    unsigned char wire[65];
    uint32_t seed = UINT32_C(0x6c8e9cf5);
    size_t length_index, i;
    int head;
    for (head = 0; head < 256; ++head) {
        wire[0] = (unsigned char)head;
        for (i = 1u; i < sizeof(wire); ++i) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            wire[i] = (unsigned char)seed;
        }
        for (length_index = 0u; length_index <
            sizeof(lengths) / sizeof(lengths[0]); ++length_index) {
            xvalue *value = xcbor_decode(wire, lengths[length_index]);
            CHECK((value == NULL) == xcbor_failed());
            xrtValueRelease(value);
            {
                xbuffer *bytes = xcbor_decode_bytes(wire, lengths[length_index]);
                CHECK((bytes == NULL) == xcbor_failed());
                xrtBufferDestroy(bytes);
            }
        }
    }
    return 0;
}

/* 从有效的嵌套容器出发，逐位损坏及截断都须保持解码诊断一致。 */
static int nested_mutation_scan(void) {
    unsigned char wire[] = {
        0xa2, 0x61, 'a', 0x9f, 0x01, 0x5f, 0x42, 0xde, 0xad,
        0x41, 0xbe, 0xff, 0xff, 0x61, 'b', 0xf6
    };
    xvalue *original = xcbor_decode(wire, sizeof(wire));
    size_t length, offset;
    unsigned bit;
    CHECK(original != NULL && xrtValueType(original) == XVALUE_OBJECT &&
        !xcbor_failed());
    xrtValueRelease(original);
    for (length = 0u; length <= sizeof(wire); ++length) {
        xvalue *value = xcbor_decode(wire, length);
        CHECK((value == NULL) == xcbor_failed());
        xrtValueRelease(value);
    }
    for (offset = 0u; offset < sizeof(wire); ++offset) {
        for (bit = 0u; bit < 8u; ++bit) {
            xvalue *value;
            wire[offset] ^= (unsigned char)(1u << bit);
            value = xcbor_decode(wire, sizeof(wire));
            CHECK((value == NULL) == xcbor_failed());
            xrtValueRelease(value);
            wire[offset] ^= (unsigned char)(1u << bit);
        }
    }
    return 0;
}

static int oversized_array_error(void) {
    xvalue* array = xrtValueArray();
    xbuffer* encoded;
    size_t i;
    CHECK(array != NULL);
    for (i = 0u; i <= 1000000u; ++i) {
        CHECK(xrtValueArrayAppend(array, xrtValueNull()));
    }
    encoded = xcbor_encode(array);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtBufferDestroy(encoded);
    xrtValueRelease(array);
    return 0;
}

/* 时间编码为 tag 0 + 文本，tag 的载荷也占一个解码深度。 */
static int tagged_time_depth(void) {
    xvalue *nested = xrtValueTime(123456);
    xbuffer *wire;
    int depth;

    CHECK(nested != NULL);
    for (depth = 0; depth < 64; ++depth) {
        xvalue *outer = xrtValueArray();
        CHECK(outer != NULL && xrtValueArrayAppendTake(outer, &nested));
        nested = outer;
        if (depth != 62 && depth != 63) continue;
        wire = xcbor_encode(nested);
        if (depth == 62) {
            xvalue *decoded;
            CHECK(wire != NULL && !xrtGetError());
            decoded = xcbor_decode(wire->Data, wire->Size);
            CHECK(decoded != NULL && xrtValueType(decoded) == XVALUE_ARRAY &&
                !xrtGetError());
            xrtValueRelease(decoded);
        } else {
            CHECK(wire == NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_RANGE);
        }
        xrtBufferDestroy(wire);
    }
    xrtValueRelease(nested);
    return 0;
}

/* 子数组分别合法时，编码也必须执行解码器的整份文档预算。 */
static int aggregate_items_error(void) {
    xvalue* root = xrtValueArray();
    xvalue* decoded;
    xbuffer* encoded;
    size_t child, i;
    CHECK(root != NULL);
    for (child = 0u; child < 2u; ++child) {
        xvalue* array = xrtValueArray();
        CHECK(array != NULL);
        for (i = 0u; i < 499999u; ++i) {
            CHECK(xrtValueArrayAppend(array, xrtValueNull()));
        }
        CHECK(xrtValueArrayAppend(root, array));
        xrtValueRelease(array);
    }
    encoded = xcbor_encode(root);
    CHECK(encoded != NULL && xrtGetError() == NULL);
    decoded = xcbor_decode(encoded->Data, encoded->Size);
    CHECK(decoded != NULL && xrtGetError() == NULL);
    xrtValueRelease(decoded);
    xrtBufferDestroy(encoded);
    for (child = 0u; child < 2u; ++child) {
        CHECK(xrtValueArrayAppend(xrtValueArrayGet(root, child), xrtValueNull()));
    }
    encoded = xcbor_encode(root);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtBufferDestroy(encoded);
    xrtValueRelease(root);
    return 0;
}

static int text_nul_roundtrip(void) {
    static const uint8 wire[] = {0xa1, 0x63, 'a', 0, 'b',
        0x63, 'x', 0, 'y'};
    static const char key[] = {'a', 0, 'b'};
    static const char text[] = {'x', 0, 'y'};
    xvalue* document = xcbor_decode(wire, sizeof(wire));
    xvalue* value;
    xbuffer* encoded;
    xstrview restored;

    CHECK(document != NULL && xrtValueType(document) == XVALUE_OBJECT);
    value = xrtValueObjectGet(document, (xstrview){key, sizeof(key)});
    CHECK(xrtValueGetString(value, &restored) && restored.Size == sizeof(text) &&
        memcmp(restored.Data, text, sizeof(text)) == 0);
    encoded = xcbor_encode(document);
    CHECK(encoded != NULL && encoded->Size == sizeof(wire) &&
        memcmp(encoded->Data, wire, sizeof(wire)) == 0);
    xrtBufferDestroy(encoded);
    xrtValueRelease(document);
    return 0;
}

/* 固定字节头验证 23/24、255/256、65535/65536 三处长度编码切换。 */
static int length_headers(void) {
    static const struct {
        size_t length;
        uint8 bytes[5];
        uint8 text[5];
        size_t head_size;
    } cases[] = {
        {23u, {0x57}, {0x77}, 1u},
        {24u, {0x58, 0x18}, {0x78, 0x18}, 2u},
        {255u, {0x58, 0xff}, {0x78, 0xff}, 2u},
        {256u, {0x59, 0x01, 0x00}, {0x79, 0x01, 0x00}, 3u},
        {65535u, {0x59, 0xff, 0xff}, {0x79, 0xff, 0xff}, 3u},
        {65536u, {0x5a, 0x00, 0x01, 0x00, 0x00},
            {0x7a, 0x00, 0x01, 0x00, 0x00}, 5u}
    };
    static uint8 payload[65536];
    size_t i;

    memset(payload, 'a', sizeof(payload));
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        xbuffer *wire = xcbor_encode_bytes(payload, cases[i].length);
        xbuffer *decoded;
        xvalue *source;
        xvalue *restored;
        xstrview text_value;

        CHECK(wire != NULL && wire->Size == cases[i].head_size + cases[i].length);
        CHECK(memcmp(wire->Data, cases[i].bytes, cases[i].head_size) == 0);
        decoded = xcbor_decode_bytes(wire->Data, wire->Size);
        CHECK(decoded != NULL && decoded->Size == cases[i].length &&
            memcmp(decoded->Data, payload, cases[i].length) == 0);
        xrtBufferDestroy(decoded);
        xrtBufferDestroy(wire);

        source = xrtValueString((xstrview){(const char *)payload, cases[i].length});
        CHECK(source != NULL);
        wire = xcbor_encode(source);
        CHECK(wire != NULL && wire->Size == cases[i].head_size + cases[i].length);
        CHECK(memcmp(wire->Data, cases[i].text, cases[i].head_size) == 0);
        restored = xcbor_decode(wire->Data, wire->Size);
        CHECK(restored != NULL && xrtValueGetString(restored, &text_value) &&
            text_value.Size == cases[i].length &&
            memcmp(text_value.Data, payload, cases[i].length) == 0);
        xrtValueRelease(restored);
        xrtBufferDestroy(wire);
        xrtValueRelease(source);
    }
    return 0;
}

int main(void) {
    CHECK(oversized_input_errors() == 0);
    static const unsigned char null_wire[] = {0xf6};
    static const unsigned char half[] = {0xf9, 0x3e, 0x00};
    static const unsigned char single[] = {0xfa, 0x3f, 0xc0, 0x00, 0x00};
    static const unsigned char negative_zero[] = {0xf9, 0x80, 0x00};
    static const unsigned char raw[] = {0x5f, 0x42, 0xde, 0xad,
        0x42, 0xbe, 0xef, 0xff};
    static const unsigned char text[] = {0x7f, 0x62, 'h', 'i',
        0x63, 0xe4, 0xbd, 0xa0, 0xff};
    static const unsigned char list[] = {0x9f, 0x01, 0x02, 0xff};
    static const unsigned char object[] = {0xbf, 0x61, 'a', 0x01, 0xff};
    static const unsigned char nested_indefinite[] = {
        0x9f, 0x01, 0x5f, 0x42, 0xde, 0xad, 0x41, 0xbe, 0xff,
        0xa1, 0x61, 'x', 0xf6, 0xff
    };
    static const unsigned char duplicate[] = {0xa2, 0x61, 'a', 0x01,
        0x61, 'a', 0x02};
    static const unsigned char bad_text[] = {0x61, 0xff};
    static const unsigned char split_utf8[] = {0x7f, 0x61, 0xe4,
        0x62, 0xbd, 0xa0, 0xff};
    static const unsigned char truncated[] = {0x5f, 0x41, 0x01};
    static const unsigned char bad_key[] = {0xa1, 0x01, 0x02};
    static const unsigned char unsupported_tag[] = {0xc2, 0x01};
    static const unsigned char unsupported_large_tag[] = {
        0xc2, 0x9a, 0x00, 0x0f, 0x42, 0x41
    };
    static const unsigned char unexpected_break[] = {0xff};
    static const unsigned char half_time[] = {0xc1, 0xf9, 0x38, 0x00};
    static const unsigned char empty_raw[] = {0x5f, 0xff};
    static const unsigned char int64_min[] = {0x3b, 0x7f, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff};
    static const unsigned char int64_max[] = {0x1b, 0x7f, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff};
    static const unsigned char uint64_max[] = {0x1b, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff};
    static const unsigned char above_int64[] = {0x1b, 0x80, 0, 0, 0, 0, 0, 0, 0};
    static const unsigned char below_int64[] = {0x3b, 0x80, 0, 0, 0, 0, 0, 0, 0};
    unsigned char max_length_with_break[] = {0x5b, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff};
    static const unsigned char max_length_heads[] = {0x5b, 0x7b, 0x9b, 0xbb};
    static const unsigned char text_time[] = {0xc0, 0x74,
        '2', '0', '2', '6', '-', '0', '7', '-', '1', '8', 'T', '1', '2', ':',
        '3', '0', ':', '0', '0', 'Z'};
    xvalue *value;
    xbytesview bytes;
    xstrview string;
    xbuffer *buffer;
    double number;
    int64 count;
    uint64 unsigned_count;
    xtime time;
    char *first, *later;
    int i;

    CHECK(oversized_array_error() == 0);
    CHECK(tagged_time_depth() == 0);
    CHECK(aggregate_items_error() == 0);
    CHECK(short_input_scan() == 0);
    CHECK(nested_mutation_scan() == 0);
    CHECK(length_headers() == 0);

    value = xcbor_decode(null_wire, sizeof(null_wire));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && !xcbor_failed());
    xrtValueRelease(value);

    value = xcbor_decode(half, sizeof(half));
    CHECK(value && xrtValueGetFloat(value, &number) && number == 1.5);
    xrtValueRelease(value);
    value = xcbor_decode(single, sizeof(single));
    CHECK(value && xrtValueGetFloat(value, &number) && number == 1.5);
    xrtValueRelease(value);
    value = xcbor_decode(negative_zero, sizeof(negative_zero));
    CHECK(value && xrtValueGetFloat(value, &number) && signbit(number));
    xrtValueRelease(value);
    value = xcbor_decode(half_time, sizeof(half_time));
    CHECK(value && xrtValueGetTime(value, &time) && time == 500000);
    buffer = xcbor_encode(value);
    CHECK(buffer != NULL);
    {
        xvalue *roundtrip = xcbor_decode(buffer->Data, buffer->Size);
        CHECK(roundtrip != NULL && xrtValueGetTime(roundtrip, &time) &&
            time == 500000);
        xrtValueRelease(roundtrip);
    }
    xrtBufferDestroy(buffer);
    xrtValueRelease(value);
    {
        const xtime moments[] = {123456, -123456, 1710000000123456};
        size_t index;
        for (index = 0; index < sizeof(moments) / sizeof(moments[0]); ++index) {
            xvalue *source = xrtValueTime(moments[index]);
            xvalue *decoded;
            CHECK(source != NULL);
            buffer = xcbor_encode(source);
            CHECK(buffer != NULL);
            decoded = xcbor_decode(buffer->Data, buffer->Size);
            CHECK(decoded != NULL && xrtValueGetTime(decoded, &time) &&
                time == moments[index]);
            xrtValueRelease(decoded);
            xrtBufferDestroy(buffer);
            xrtValueRelease(source);
        }
    }
    value = xcbor_decode(text_time, sizeof(text_time));
    CHECK(value && xrtValueType(value) == XVALUE_TIME);
    buffer = xcbor_encode(value);
    CHECK(buffer && buffer->Size == sizeof(text_time) &&
        memcmp(buffer->Data, text_time, sizeof(text_time)) == 0);
    xrtBufferDestroy(buffer);
    xrtValueRelease(value);
    value = xcbor_decode(int64_min, sizeof(int64_min));
    CHECK(value && xrtValueGetInt(value, &count) && count == INT64_MIN);
    xrtValueRelease(value);
    value = xcbor_decode(int64_max, sizeof(int64_max));
    CHECK(value && xrtValueGetInt(value, &count) && count == INT64_MAX);
    xrtValueRelease(value);
    value = xcbor_decode(above_int64, sizeof(above_int64));
    CHECK(value && xrtValueType(value) == XVALUE_UINT &&
        xrtValueGetUInt(value, &unsigned_count) &&
        unsigned_count == (uint64)INT64_MAX + 1u);
    xrtValueRelease(value);
    value = xcbor_decode(uint64_max, sizeof(uint64_max));
    CHECK(value && xrtValueType(value) == XVALUE_UINT &&
        xrtValueGetUInt(value, &unsigned_count) && unsigned_count == UINT64_MAX);
    xrtValueRelease(value);
    value = xrtValueUInt(UINT64_MAX);
    CHECK(value != NULL);
    buffer = xcbor_encode(value);
    CHECK(buffer && buffer->Size == sizeof(uint64_max) &&
        memcmp(buffer->Data, uint64_max, sizeof(uint64_max)) == 0);
    xrtBufferDestroy(buffer);
    xrtValueRelease(value);
    value = xcbor_decode(below_int64, sizeof(below_int64));
    CHECK(value == NULL && xrtGetError() &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();

    value = xcbor_decode(raw, sizeof(raw));
    CHECK(value && xrtValueGetBytes(value, &bytes) && bytes.Size == 4 &&
        memcmp(bytes.Data, "\xde\xad\xbe\xef", 4) == 0);
    xrtValueRelease(value);
    buffer = xcbor_decode_bytes(raw, sizeof(raw));
    CHECK(buffer && buffer->Size == 4 &&
        memcmp(buffer->Data, "\xde\xad\xbe\xef", 4) == 0);
    xrtBufferDestroy(buffer);
    buffer = xcbor_decode_bytes(empty_raw, sizeof(empty_raw));
    CHECK(buffer && buffer->Size == 0);
    xrtBufferDestroy(buffer);

    value = xcbor_decode(text, sizeof(text));
    CHECK(value && xrtValueGetString(value, &string) && string.Size == 5 &&
        memcmp(string.Data, "hi你", 5) == 0);
    xrtValueRelease(value);
    value = xcbor_decode(list, sizeof(list));
    CHECK(value && xrtValueType(value) == XVALUE_ARRAY && xrtValueCount(value) == 2);
    xrtValueRelease(value);
    value = xcbor_decode(object, sizeof(object));
    CHECK(value && xrtValueType(value) == XVALUE_OBJECT &&
        xrtValueGetInt(xrtValueObjectGet(value, XRT_STR_LITERAL("a")), &count) &&
        count == 1);
    xrtValueRelease(value);
    CHECK(text_nul_roundtrip() == 0);

    /* 外层 break 到达前，内层已闭合也不能交出半个文档。 */
    for (i = 0; i < (int)sizeof(nested_indefinite); ++i)
        CHECK(invalid(nested_indefinite, (size_t)i));
    value = xcbor_decode(nested_indefinite, sizeof(nested_indefinite));
    CHECK(value != NULL && xrtValueType(value) == XVALUE_ARRAY &&
        xrtValueCount(value) == 3u && !xcbor_failed());
    CHECK(xrtValueGetInt(xrtValueArrayAt(value, 0u), &count) && count == 1);
    CHECK(xrtValueGetBytes(xrtValueArrayAt(value, 1u), &bytes) &&
        bytes.Size == 3u && memcmp(bytes.Data, "\xde\xad\xbe", 3u) == 0);
    CHECK(xrtValueType(xrtValueObjectGet(xrtValueArrayAt(value, 2u),
        XRT_STR_LITERAL("x"))) == XVALUE_NULL);
    xrtValueRelease(value);

    CHECK(invalid(duplicate, sizeof(duplicate)));
    CHECK(invalid(bad_text, sizeof(bad_text)));
    CHECK(invalid(split_utf8, sizeof(split_utf8)));
    CHECK(invalid(truncated, sizeof(truncated)));
    CHECK(invalid(bad_key, sizeof(bad_key)));
    CHECK(invalid(unsupported_tag, sizeof(unsupported_tag)));
    value = xcbor_decode(unsupported_large_tag, sizeof(unsupported_large_tag));
    CHECK(value == NULL && xcbor_failed());
    CHECK(strcmp(xcbor_error_message(), "unsupported CBOR semantic tag") == 0);
    xrtClearError();
    CHECK(invalid(unexpected_break, sizeof(unexpected_break)));
    for (i = 0; i < (int)(sizeof(max_length_heads) / sizeof(max_length_heads[0])); ++i) {
        max_length_with_break[0] = max_length_heads[i];
        value = xcbor_decode(max_length_with_break, sizeof(max_length_with_break));
        CHECK(value == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtClearError();
    }

    value = xcbor_decode_abi((void*)bad_text, sizeof(bad_text));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && xcbor_failed());
    xrtValueRelease(value);
    value = xcbor_decode_abi((void*)null_wire, sizeof(null_wire));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && !xcbor_failed());
    xrtValueRelease(value);

    first = xcbor_to_hex(raw, sizeof(raw));
    CHECK(first && strcmp(first, "5f42dead42beefff") == 0);
    for (i = 0; i < 20; ++i) {
        later = xcbor_to_hex(half, sizeof(half));
        CHECK(later && strcmp(later, "f93e00") == 0);
        xrtFree(later);
    }
    CHECK(strcmp(first, "5f42dead42beefff") == 0);
    xrtFree(first);
    first = xcbor_to_hex(NULL, 0);
    CHECK(first && strcmp(first, "") == 0);
    xrtFree(first);
    {
        size_t size = 2u * 64u * 1024u * 1024u + 1u;
        char *oversized = (char *)malloc(size);
        CHECK(oversized != NULL);
        memset(oversized, 'a', size);
        buffer = xcbor_from_hex(oversized);
        CHECK(buffer == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE);
        free(oversized);
    }
    {
        uint32_t state = 0x9e3779b9u;
        unsigned char wire[16];
        size_t sample, j;
        for (sample = 0; sample < 20000; sample++) {
            size_t size = sample % sizeof(wire) + 1;
            xvalue *decoded;
            for (j = 0; j < size; j++) {
                state = state * 1664525u + 1013904223u;
                wire[j] = (unsigned char)(state >> 24);
            }
            decoded = xcbor_decode(wire, size);
            CHECK((decoded == NULL) == xcbor_failed());
            xrtValueRelease(decoded);
            xrtClearError();
        }
    }
    return 0;
}
