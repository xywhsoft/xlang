/* 故障注入覆盖六个公开入口和嵌套值的清理路径。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../msgpack_native.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "MessagePack allocation test failed at %d: %s\n", \
        __LINE__, #condition); \
    return 1; \
} } while (0)

static const unsigned char nested_wire[] = {
    0x92, 0x81, 0xa4, 'n', 'a', 'm', 'e', 0xa5, 'x', 'l', 'a', 'n', 'g',
    0x81, 0xa1, 'v', 0xc4, 0x04, 0xde, 0xad, 0xbe, 0xef
};
static const unsigned char bytes_wire[] = {0xc4, 0x04, 0xde, 0xad, 0xbe, 0xef};
static const unsigned char raw_bytes[] = {0xde, 0xad, 0xbe, 0xef};
static const unsigned char maximum_unsigned_wire[] = {
    0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void* operate(unsigned int operation, xvalue* source)
{
    switch (operation) {
        case 0: return xmsgpack_encode(source);
        case 1: return xmsgpack_decode(nested_wire, sizeof(nested_wire));
        case 2: return xmsgpack_encode_bytes(raw_bytes, sizeof(raw_bytes));
        case 3: return xmsgpack_decode_bytes(bytes_wire, sizeof(bytes_wire));
        case 4: return xmsgpack_to_hex(nested_wire, sizeof(nested_wire));
        default: return xmsgpack_from_hex("c404deadbeef");
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
        CHECK(xrtMemDebugFailAfter(0));
        unsigned_value = xmsgpack_decode(maximum_unsigned_wire,
            sizeof(maximum_unsigned_wire));
        CHECK(unsigned_value == NULL && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        unsigned_value = xmsgpack_decode(maximum_unsigned_wire,
            sizeof(maximum_unsigned_wire));
        CHECK(unsigned_value && xrtValueGetUInt(unsigned_value, &number) &&
            number == UINT64_MAX && xrtGetError() == NULL);
        xrtValueRelease(unsigned_value);
    }
    xrtValueRelease(source);
    puts("msgpack allocation failures: PASS");
    return 0;
}
