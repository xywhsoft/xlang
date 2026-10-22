/* 逐个分配点验证 CBOR C API 的失败原子性和错误分类。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../cbor_native.h"

#include <stdio.h>

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

static void* operate(unsigned int operation, xvalue* source)
{
    switch (operation) {
        case 0: return xcbor_encode(source);
        case 1: return xcbor_decode(nested_wire, sizeof(nested_wire));
        case 2: return xcbor_encode_bytes(raw_bytes, sizeof(raw_bytes));
        case 3: return xcbor_decode_bytes(bytes_wire, sizeof(bytes_wire));
        case 4: return xcbor_to_hex(nested_wire, sizeof(nested_wire));
        default: return xcbor_from_hex("44deadbeef");
    }
}

static void release_result(unsigned int operation, void* result)
{
    if (operation == 1u) xrtValueRelease(result);
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
    for (operation = 0; operation < 6u; ++operation) {
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
