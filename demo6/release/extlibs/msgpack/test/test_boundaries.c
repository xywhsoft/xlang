#define XRT_MODULE_ALL
#include "xrt.h"
#include "../msgpack_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "MessagePack boundary failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int invalid(const unsigned char* data, size_t size) {
    xvalue* value = xmsgpack_decode(data, size);
    int rejected = value == NULL && xmsgpack_has_error();
    xrtValueRelease(value);
    xrtClearError();
    return rejected;
}

static int null_value_contract(void) {
    xbuffer* encoded = xmsgpack_encode(NULL);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    encoded = xmsgpack_encode(xrtValueNull());
    CHECK(encoded != NULL && encoded->Size == 1u &&
        encoded->Data[0] == 0xc0u && xrtGetError() == NULL);
    xrtBufferDestroy(encoded);
    return 0;
}

static int unsigned_integer_roundtrip(void) {
    static const uint8 small_wire[] = {0xccu, 0x80u};
    static const uint8 signed_maximum_wire[] = {
        0xcfu, 0x7fu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu
    };
    static const uint8 maximum_wire[] = {
        0xcfu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu
    };
    static const uint8 above_signed_wire[] = {
        0xcfu, 0x80u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
    };
    xvalue* source = xrtValueUInt(128u);
    xvalue* decoded;
    xbuffer* wire;
    uint64 number = 0u;
    int64 signed_number = 0;
    CHECK(source != NULL);
    wire = xmsgpack_encode(source);
    CHECK(wire && wire->Size == sizeof(small_wire) &&
        memcmp(wire->Data, small_wire, sizeof(small_wire)) == 0);
    decoded = xmsgpack_decode(small_wire, sizeof(small_wire));
    CHECK(decoded && xrtValueType(decoded) == XVALUE_INT &&
        xrtValueGetInt(decoded, &signed_number) && signed_number == 128);
    xrtValueRelease(decoded);
    xrtBufferDestroy(wire);
    xrtValueRelease(source);

    decoded = xmsgpack_decode(signed_maximum_wire, sizeof(signed_maximum_wire));
    CHECK(decoded && xrtValueType(decoded) == XVALUE_INT &&
        xrtValueGetInt(decoded, &signed_number) && signed_number == INT64_MAX);
    xrtValueRelease(decoded);

    source = xrtValueUInt(UINT64_MAX);
    CHECK(source != NULL);
    wire = xmsgpack_encode(source);
    CHECK(wire && wire->Size == sizeof(maximum_wire) &&
        memcmp(wire->Data, maximum_wire, sizeof(maximum_wire)) == 0);
    decoded = xmsgpack_decode(maximum_wire, sizeof(maximum_wire));
    CHECK(decoded && xrtValueType(decoded) == XVALUE_UINT &&
        xrtValueGetUInt(decoded, &number) && number == UINT64_MAX);
    xrtValueRelease(decoded);
    xrtBufferDestroy(wire);
    xrtValueRelease(source);

    decoded = xmsgpack_decode(above_signed_wire, sizeof(above_signed_wire));
    CHECK(decoded && xrtValueType(decoded) == XVALUE_UINT &&
        xrtValueGetUInt(decoded, &number) && number == (uint64)INT64_MAX + 1u);
    xrtValueRelease(decoded);
    return 0;
}

/* 线格式长度阈值必须与解码器一致，不能只验证本实现自生成的头部。 */
static int length_headers(void) {
    static const size_t text_sizes[] = {31u, 32u, 255u, 256u, 65535u, 65536u};
    static const uint8 text_headers[][5] = {
        {0xbfu}, {0xd9u, 0x20u}, {0xd9u, 0xffu},
        {0xdau, 0x01u, 0x00u}, {0xdau, 0xffu, 0xffu},
        {0xdbu, 0x00u, 0x01u, 0x00u, 0x00u}
    };
    static const size_t text_header_sizes[] = {1u, 2u, 2u, 3u, 3u, 5u};
    static const size_t binary_sizes[] = {255u, 256u, 65535u, 65536u};
    static const uint8 binary_headers[][5] = {
        {0xc4u, 0xffu}, {0xc5u, 0x01u, 0x00u},
        {0xc5u, 0xffu, 0xffu}, {0xc6u, 0x00u, 0x01u, 0x00u, 0x00u}
    };
    static const size_t binary_header_sizes[] = {2u, 3u, 3u, 5u};
    char* text = (char*)malloc(65536u);
    size_t i;
    CHECK(text != NULL);
    memset(text, 'a', 65536u);
    for (i = 0u; i < sizeof(text_sizes) / sizeof(text_sizes[0]); ++i) {
        xvalue* source = xrtValueString((xstrview){text, text_sizes[i]});
        xbuffer* wire = source == NULL ? NULL : xmsgpack_encode(source);
        xvalue* decoded = wire == NULL ? NULL : xmsgpack_decode(wire->Data, wire->Size);
        xstrview result;
        CHECK(source && wire && decoded &&
            wire->Size == text_header_sizes[i] + text_sizes[i] &&
            memcmp(wire->Data, text_headers[i], text_header_sizes[i]) == 0 &&
            xrtValueGetString(decoded, &result) && result.Size == text_sizes[i] &&
            memcmp(result.Data, text, result.Size) == 0);
        xrtValueRelease(decoded);
        xrtBufferDestroy(wire);
        xrtValueRelease(source);
    }
    for (i = 0u; i < sizeof(binary_sizes) / sizeof(binary_sizes[0]); ++i) {
        xbuffer* wire = xmsgpack_encode_bytes((const uint8*)text, binary_sizes[i]);
        xbuffer* decoded = wire == NULL ? NULL : xmsgpack_decode_bytes(wire->Data, wire->Size);
        CHECK(wire && decoded &&
            wire->Size == binary_header_sizes[i] + binary_sizes[i] &&
            memcmp(wire->Data, binary_headers[i], binary_header_sizes[i]) == 0 &&
            decoded->Size == binary_sizes[i] &&
            memcmp(decoded->Data, text, decoded->Size) == 0);
        xrtBufferDestroy(decoded);
        xrtBufferDestroy(wire);
    }
    free(text);
    return 0;
}

static int oversized_input_errors(void) {
    size_t size = 64u * 1024u * 1024u + 1u;
    unsigned char* input = (unsigned char*)malloc(size);
    xvalue* value;
    xbuffer* buffer;
    str hex;
    CHECK(input != NULL);
    memset(input, 0, size);
    value = xmsgpack_decode(input, size);
    CHECK(value == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    buffer = xmsgpack_decode_bytes(input, size);
    CHECK(buffer == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    buffer = xmsgpack_encode_bytes(input, size);
    CHECK(buffer == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    hex = xmsgpack_to_hex(input, size);
    CHECK(hex == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    free(input);
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
    encoded = xmsgpack_encode(array);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtBufferDestroy(encoded);
    xrtValueRelease(array);
    return 0;
}

/* 每个子数组都合法，但整个文档的容器项数超过解码器预算。 */
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
    encoded = xmsgpack_encode(root);
    CHECK(encoded != NULL && xrtGetError() == NULL);
    decoded = xmsgpack_decode(encoded->Data, encoded->Size);
    CHECK(decoded != NULL && xrtGetError() == NULL);
    xrtValueRelease(decoded);
    xrtBufferDestroy(encoded);
    for (child = 0u; child < 2u; ++child) {
        CHECK(xrtValueArrayAppend(xrtValueArrayGet(root, child), xrtValueNull()));
    }
    encoded = xmsgpack_encode(root);
    CHECK(encoded == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtBufferDestroy(encoded);
    xrtValueRelease(root);
    return 0;
}

static int timestamp_roundtrip(const unsigned char* wire, size_t size, xtime expected) {
    xvalue* value = xmsgpack_decode(wire, size);
    xbuffer* encoded;
    xtime moment;
    CHECK(value && xrtValueGetTime(value, &moment) && moment == expected);
    encoded = xmsgpack_encode(value);
    CHECK(encoded && encoded->Size == size && memcmp(encoded->Data, wire, size) == 0);
    xrtBufferDestroy(encoded);
    xrtValueRelease(value);
    return 0;
}

static int text_nul_roundtrip(void) {
    static const unsigned char wire[] = {0x81, 0xa3, 'a', 0, 'b',
        0xa3, 'x', 0, 'y'};
    static const char key[] = {'a', 0, 'b'};
    static const char text[] = {'x', 0, 'y'};
    xvalue* document = xmsgpack_decode(wire, sizeof(wire));
    xvalue* value;
    xbuffer* encoded;
    xstrview restored;

    CHECK(document != NULL && xrtValueType(document) == XVALUE_OBJECT);
    value = xrtValueObjectGet(document, (xstrview){key, sizeof(key)});
    CHECK(xrtValueGetString(value, &restored) && restored.Size == sizeof(text) &&
        memcmp(restored.Data, text, sizeof(text)) == 0);
    encoded = xmsgpack_encode(document);
    CHECK(encoded != NULL && encoded->Size == sizeof(wire) &&
        memcmp(encoded->Data, wire, sizeof(wire)) == 0);
    xrtBufferDestroy(encoded);
    xrtValueRelease(document);
    return 0;
}

/* 固定边界加固定种子，覆盖负秒数的微秒余数和时间戳布局切换。 */
static int timestamp_matrix(void) {
    static const xtime boundaries[] = {
        INT64_MIN, INT64_MIN + 1, -1000001, -1000000, -999999, -1,
        0, 1, 999999, 1000000, 1000001,
        INT64_C(4294967295) * 1000000,
        INT64_C(4294967296) * 1000000,
        INT64_MAX - 1, INT64_MAX
    };
    uint64_t seed = UINT64_C(0x9e3779b97f4a7c15);
    size_t i;
    for (i = 0; i < 4096 + sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
        xtime expected;
        xtime actual;
        xvalue* source;
        xvalue* decoded;
        xbuffer* encoded;
        if (i < sizeof(boundaries) / sizeof(boundaries[0])) {
            expected = boundaries[i];
        } else {
            seed = seed * UINT64_C(6364136223846793005) + 1;
            expected = (xtime)(seed % UINT64_C(18000000000000000)) -
                INT64_C(9000000000000000);
        }
        source = xrtValueTime(expected);
        CHECK(source != NULL);
        encoded = xmsgpack_encode(source);
        CHECK(encoded != NULL && xrtGetError() == NULL);
        decoded = xmsgpack_decode(encoded->Data, encoded->Size);
        CHECK(decoded != NULL && xrtGetError() == NULL &&
            xrtValueGetTime(decoded, &actual) && actual == expected);
        xrtValueRelease(decoded);
        xrtBufferDestroy(encoded);
        xrtValueRelease(source);
    }
    return 0;
}

static int map_allocation_errors(void) {
    static const unsigned char map[] = {0x81, 0xa1, 'k', 1};
    unsigned failure, rejected = 0u, succeeded = 0u;
    for (failure = 0u; failure < 32u && succeeded < 2u; ++failure) {
        xvalue* value;
        int triggered;
        CHECK(xrtMemDebugFailAfter(failure));
        value = xmsgpack_decode(map, sizeof(map));
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (value != NULL || xrtGetError() == NULL ||
                xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "map allocation point %u: error kind %d\n",
                    failure, xrtGetError() == NULL ? -1 :
                    (int)xrtErrorKind(xrtGetError()));
            CHECK(value == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            CHECK(value != NULL && xrtGetError() == NULL);
            ++succeeded;
        }
        xrtValueRelease(value);
        xrtClearError();
    }
    CHECK(rejected >= 2u && succeeded == 2u);
    return 0;
}

static int mutation_result(const unsigned char *data, size_t size) {
    xvalue *value = xmsgpack_decode(data, size);
    int consistent = value != NULL ? !xmsgpack_has_error() : xmsgpack_has_error();
    xrtValueRelease(value);
    return consistent;
}

/* 逐个覆盖所有线格式首字节及短输入长度，固定失败诊断合同。 */
static int header_matrix(void) {
    unsigned char wire[18] = {0};
    unsigned head;
    size_t size;
    for (head = 0; head <= 255u; ++head) {
        wire[0] = (unsigned char)head;
        for (size = 1u; size <= sizeof(wire); ++size) {
            if (!mutation_result(wire, size)) {
                fprintf(stderr, "MessagePack header %02x size %zu has inconsistent error\n",
                    head, size);
                return 1;
            }
        }
    }
    return 0;
}

/* 复合值截断与单比特损坏不得越界，也不能丢失失败诊断。 */
static int mutation_matrix(void) {
    unsigned char wire[] = {
        0x82, 0xa1, 'a', 0x92, 0xc0, 0x2a,
        0xa1, 'b', 0xc4, 0x02, 0x00, 0xff
    };
    xvalue *sample = xmsgpack_decode(wire, sizeof(wire));
    size_t size, offset;
    int bit;
    CHECK(sample != NULL && xrtValueType(sample) == XVALUE_OBJECT);
    xrtValueRelease(sample);
    for (size = 0; size <= sizeof(wire); ++size)
        CHECK(mutation_result(wire, size));
    for (offset = 0; offset < sizeof(wire); ++offset) {
        for (bit = 0; bit < 8; ++bit) {
            wire[offset] ^= (unsigned char)(1u << bit);
            CHECK(mutation_result(wire, sizeof(wire)));
            wire[offset] ^= (unsigned char)(1u << bit);
        }
    }
    return 0;
}

int main(void) {
    CHECK(null_value_contract() == 0);
    CHECK(unsigned_integer_roundtrip() == 0);
    CHECK(length_headers() == 0);
    CHECK(oversized_input_errors() == 0);
    CHECK(header_matrix() == 0);
    CHECK(mutation_matrix() == 0);
    CHECK(text_nul_roundtrip() == 0);
    CHECK(oversized_array_error() == 0);
    CHECK(aggregate_items_error() == 0);
    static const unsigned char nil[] = {0xc0};
    static const unsigned char float32[] = {0xca, 0x3f, 0xc0, 0, 0};
    static const unsigned char raw[] = {0xc4, 2, 0, 0xff};
    static const unsigned char duplicate[] = {0x82, 0xa1, 'a', 1, 0xa1, 'a', 2};
    static const unsigned char invalid_utf8[] = {0xa1, 0xff};
    static const unsigned char truncated[] = {0xd3, 0, 0};
    static const unsigned char truncated_uint64[] = {0xcf, 0, 0};
    static const unsigned char trailing[] = {1, 2};
    static const unsigned char nontext_key[] = {0x81, 1, 2};
    static const unsigned char unsupported[] = {0xc1};
    static const unsigned char oversized_text[] = {0xdb, 0x04, 0x00, 0x00, 0x01};
    static const unsigned char timestamp32[] = {0xd6, 0xff, 0, 0, 0, 0};
    static const unsigned char timestamp64[] = {
        0xd7, 0xff, 0, 0, 0x0f, 0xa0, 0, 0, 0, 0
    };
    static const unsigned char timestamp96[] = {
        0xc7, 12, 0xff, 0x1d, 0xcd, 0x65, 0,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    static const unsigned char timestamp64_long[] = {
        0xd7, 0xff, 0, 0, 0, 1, 0, 0, 0, 0
    };
    static const unsigned char timestamp96_min[] = {
        0xc7, 12, 0xff, 0x0d, 0x5c, 0xe6, 0,
        0xff, 0xff, 0xf7, 0x9c, 0x84, 0x2f, 0xa5, 0x09
    };
    static const unsigned char timestamp96_max[] = {
        0xc7, 12, 0xff, 0x2e, 0x3d, 0xe0, 0x18,
        0, 0, 0x08, 0x63, 0x7b, 0xd0, 0x5a, 0xf6
    };
    static const unsigned char submicro[] = {
        0xd7, 0xff, 0, 0, 0, 4, 0, 0, 0, 0
    };
    static const unsigned char bad_nanos[] = {
        0xc7, 12, 0xff, 0x3b, 0x9a, 0xca, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    static const unsigned char unknown_extension[] = {0xd6, 1, 0, 0, 0, 0};
    static const unsigned char before_min_time[] = {
        0xc7, 12, 0xff, 0x0d, 0x5c, 0xe2, 0x18,
        0xff, 0xff, 0xf7, 0x9c, 0x84, 0x2f, 0xa5, 0x09
    };
    static const unsigned char after_max_time[] = {
        0xc7, 12, 0xff, 0x2e, 0x3d, 0xe4, 0,
        0, 0, 0x08, 0x63, 0x7b, 0xd0, 0x5a, 0xf6
    };
    xvalue* value;
    xbytesview bytes;
    xbuffer* buffer;
    char *first, *second;
    double number;
    unsigned char noise[40];
    uint32_t seed = 0x52345678u;
    size_t i, j;

    CHECK(map_allocation_errors() == 0);

    value = xmsgpack_decode(nil, sizeof(nil));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && !xrtGetError());
    xrtValueRelease(value);
    value = xmsgpack_decode(float32, sizeof(float32));
    CHECK(value && xrtValueGetFloat(value, &number) && number == 1.5);
    xrtValueRelease(value);
    CHECK(timestamp_roundtrip(timestamp32, sizeof(timestamp32), 0) == 0);
    CHECK(timestamp_roundtrip(timestamp64, sizeof(timestamp64), 1) == 0);
    CHECK(timestamp_roundtrip(timestamp64_long, sizeof(timestamp64_long),
        INT64_C(4294967296000000)) == 0);
    CHECK(timestamp_roundtrip(timestamp96, sizeof(timestamp96), -500000) == 0);
    CHECK(timestamp_roundtrip(timestamp96_min, sizeof(timestamp96_min), INT64_MIN) == 0);
    CHECK(timestamp_roundtrip(timestamp96_max, sizeof(timestamp96_max), INT64_MAX) == 0);
    CHECK(timestamp_matrix() == 0);
    value = xmsgpack_decode(raw, sizeof(raw));
    CHECK(value && xrtValueGetBytes(value, &bytes) && bytes.Size == 2 &&
        bytes.Data[0] == 0 && bytes.Data[1] == 0xff);
    xrtValueRelease(value);
    buffer = xmsgpack_decode_bytes(raw, sizeof(raw));
    CHECK(buffer && buffer->Size == 2 && buffer->Data[1] == 0xff);
    xrtBufferDestroy(buffer);

    first = xmsgpack_to_hex(raw, sizeof(raw));
    second = xmsgpack_to_hex(raw, sizeof(raw));
    CHECK(first && second && first != second && strcmp(first, "c40200ff") == 0 &&
        strcmp(first, second) == 0);
    xrtFree(first);
    xrtFree(second);
    value = xmsgpack_decode(invalid_utf8, sizeof(invalid_utf8));
    CHECK(value == NULL && xmsgpack_has_error());
    first = xmsgpack_to_hex(raw, sizeof(raw));
    CHECK(first && strcmp(first, "c40200ff") == 0 && !xmsgpack_has_error());
    xrtFree(first);
    buffer = xmsgpack_from_hex("c40200ff");
    CHECK(buffer && buffer->Size == sizeof(raw) &&
        memcmp(buffer->Data, raw, sizeof(raw)) == 0);
    xrtBufferDestroy(buffer);
    {
        size_t size = 2u * 64u * 1024u * 1024u + 1u;
        char *oversized = (char *)malloc(size);
        CHECK(oversized != NULL);
        memset(oversized, 'a', size);
        buffer = xmsgpack_from_hex(oversized);
        CHECK(buffer == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE);
        free(oversized);
    }

    CHECK(invalid(duplicate, sizeof(duplicate)));
    CHECK(invalid(invalid_utf8, sizeof(invalid_utf8)));
    CHECK(invalid(truncated, sizeof(truncated)));
    value = xmsgpack_decode(truncated_uint64, sizeof(truncated_uint64));
    CHECK(value == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        strstr(xmsgpack_error_message(), "truncated") != NULL);
    xrtClearError();
    CHECK(invalid(trailing, sizeof(trailing)));
    CHECK(invalid(nontext_key, sizeof(nontext_key)));
    CHECK(invalid(unsupported, sizeof(unsupported)));
    CHECK(invalid(submicro, sizeof(submicro)));
    CHECK(invalid(bad_nanos, sizeof(bad_nanos)));
    CHECK(invalid(unknown_extension, sizeof(unknown_extension)));
    CHECK(invalid(before_min_time, sizeof(before_min_time)));
    CHECK(invalid(after_max_time, sizeof(after_max_time)));
    value = xmsgpack_decode(oversized_text, sizeof(oversized_text));
    CHECK(value == NULL && xmsgpack_has_error() &&
        strstr(xmsgpack_error_message(), "safety limit") != NULL);
    xrtClearError();
    {
        unsigned char nested[67];
        memset(nested, 0x91, sizeof(nested));
        nested[sizeof(nested) - 1u] = 0xc0;
        value = xmsgpack_decode(nested, sizeof(nested));
        CHECK(value == NULL && xmsgpack_has_error() &&
            strstr(xmsgpack_error_message(), "nesting depth") != NULL);
        xrtClearError();
    }

    value = xmsgpack_decode_abi((void*)invalid_utf8, sizeof(invalid_utf8));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && xmsgpack_has_error());
    xrtValueRelease(value);
    value = xmsgpack_decode_abi((void*)nil, sizeof(nil));
    CHECK(value && xrtValueType(value) == XVALUE_NULL && !xmsgpack_has_error());
    xrtValueRelease(value);

    /* 固定种子畸形样本可重复，供 ASan/UBSan 检查解析边界。 */
    for (i = 0; i < 5000; ++i) {
        size_t size = i % sizeof(noise);
        for (j = 0; j < size; ++j) {
            seed = seed * 1664525u + 1013904223u;
            noise[j] = (unsigned char)(seed >> 24);
        }
        value = xmsgpack_decode(noise, size);
        if ((value == NULL) != xmsgpack_has_error()) {
            fprintf(stderr, "MessagePack missing error at sample %zu, size %zu:", i, size);
            for (j = 0; j < size; ++j) fprintf(stderr, " %02x", noise[j]);
            fputc('\n', stderr);
        }
        CHECK((value == NULL) == xmsgpack_has_error());
        xrtValueRelease(value);
    }
    puts("msgpack boundaries: ok");
    return 0;
}
