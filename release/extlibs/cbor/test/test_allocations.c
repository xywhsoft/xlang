/* 逐个分配点验证 CBOR C API 的失败原子性和错误分类。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../cbor_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "CBOR allocation test failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static const unsigned char nested_wire[] = {
    0x82, 0xa1, 0x64, 'n', 'a', 'm', 'e', 0x65, 'x', 'l', 'a', 'n', 'g',
    0xa1, 0x61, 'v', 0x44, 0xde, 0xad, 0xbe, 0xef
};
static const unsigned char bytes_wire[] = {0x44, 0xde, 0xad, 0xbe, 0xef};
static const unsigned char raw_bytes[] = {0xde, 0xad, 0xbe, 0xef};
static const unsigned char maximum_unsigned_wire[] = {
    0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
static const unsigned char chunked_bytes_wire[] = {
    0x5f, 0x42, 0x00, 0xff, 0x41, 0xaa, 0xff
};
static const unsigned char chunked_text_wire[] = {
    0x7f, 0x61, 'A', 0x63, 0xe4, 0xbd, 0xa0, 0xff
};
static const unsigned char empty_bytes_wire[] = {0x5f, 0xff};
static const unsigned char empty_text_wire[] = {0x7f, 0xff};

static void* operate(unsigned int operation, xvalue* source)
{
    switch (operation) {
        case 0: return xcbor_encode(source);
        case 1: return xcbor_decode(nested_wire, sizeof(nested_wire));
        case 2: return xcbor_encode_bytes(raw_bytes, sizeof(raw_bytes));
        case 3: return xcbor_decode_bytes(bytes_wire, sizeof(bytes_wire));
        case 4: return xcbor_to_hex(nested_wire, sizeof(nested_wire));
        case 5: return xcbor_from_hex("44deadbeef");
        case 6: return xcbor_decode(chunked_bytes_wire, sizeof(chunked_bytes_wire));
        case 7: return xcbor_decode(chunked_text_wire, sizeof(chunked_text_wire));
        case 8: return xcbor_decode(empty_bytes_wire, sizeof(empty_bytes_wire));
        case 9: return xcbor_decode(empty_text_wire, sizeof(empty_text_wire));
        default: return xcbor_decode_bytes(chunked_bytes_wire, sizeof(chunked_bytes_wire));
    }
}

static void release_result(unsigned int operation, void* result)
{
    if (operation == 1u || (operation >= 6u && operation <= 9u)) xrtValueRelease(result);
    else if (operation == 4u) xrtFree(result);
    else xrtBufferDestroy(result);
}

int main(void)
{
    xvalue* source = xrtValueObject();
    xvalue* name = xrtValueString(XRT_STR_LITERAL("xlang"));
    unsigned int operation;

    CHECK(source != NULL && name != NULL);
    CHECK(xrtValueObjectSetTake(source, XRT_STR_LITERAL("name"), &name));
    for (operation = 0; operation < 11u; ++operation) {
        unsigned int failure;
        unsigned int triggered_count = 0u;
        bool completed = false;
        for (failure = 0; failure < 128u; ++failure) {
            void* result;
            bool triggered;
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(failure));
            result = operate(operation, source);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++triggered_count;
                CHECK(result == NULL);
                CHECK(xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                CHECK(result != NULL && xrtGetError() == NULL);
                if (operation == 6u || operation == 8u) {
                    xbytesview data;
                    CHECK(xrtValueGetBytes(result, &data));
                    if (operation == 6u) {
                        const unsigned char expected[] = {0x00, 0xff, 0xaa};
                        CHECK(data.Size == sizeof(expected) &&
                            memcmp(data.Data, expected, sizeof(expected)) == 0);
                    } else CHECK(data.Size == 0u);
                }
                if (operation == 7u || operation == 9u) {
                    xstrview text;
                    CHECK(xrtValueGetString(result, &text));
                    if (operation == 7u) {
                        const char expected[] = "A\xe4\xbd\xa0";
                        CHECK(text.Size == sizeof(expected) - 1u &&
                            memcmp(text.Data, expected, text.Size) == 0);
                    } else CHECK(text.Size == 0u);
                }
                if (operation == 10u) {
                    xbuffer* bytes = (xbuffer*)result;
                    const unsigned char expected[] = {0x00, 0xff, 0xaa};
                    CHECK(bytes->Size == sizeof(expected) &&
                        memcmp(bytes->Data, expected, sizeof(expected)) == 0);
                }
                completed = true;
            }
            release_result(operation, result);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            if (after.LiveCount != before.LiveCount ||
                after.LiveBytes != before.LiveBytes) {
                fprintf(stderr, "CBOR operation %u allocation %u leaked: %zu -> %zu\n",
                    operation, failure, before.LiveCount, after.LiveCount);
                return 1;
            }
            if (completed) break;
        }
        CHECK(triggered_count > 0u && completed);
        {
            void* retried = operate(operation, source);
            CHECK(retried != NULL && xrtGetError() == NULL);
            release_result(operation, retried);
        }
    }
    {
        xvalue* unsigned_value;
        uint64 number = 0u;
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(0));
        unsigned_value = xcbor_decode(maximum_unsigned_wire,
            sizeof(maximum_unsigned_wire));
        CHECK(unsigned_value == NULL && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        unsigned_value = xcbor_decode(maximum_unsigned_wire,
            sizeof(maximum_unsigned_wire));
        CHECK(unsigned_value && xrtValueGetUInt(unsigned_value, &number) &&
            number == UINT64_MAX && xrtGetError() == NULL);
        xrtValueRelease(unsigned_value);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        CHECK(after.LiveCount == before.LiveCount &&
            after.LiveBytes == before.LiveBytes);
    }
    xrtValueRelease(source);
    puts("cbor allocation failures: PASS");
    return 0;
}
