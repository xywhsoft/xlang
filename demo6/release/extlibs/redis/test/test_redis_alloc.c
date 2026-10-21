#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_BUFFER
#include "xrt.h"
#include "../include/redis_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "redis allocation test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_encode(void)
{
    static const char expected[] = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    xvalue* args = xrtValueArray();
    xvalue* item = xrtValueString(XRT_STR_LITERAL("SET"));
    unsigned failure, rejected = 0u, succeeded = 0u;
    CHECK(args != NULL && item != NULL && xrtValueArrayAppendTake(args, &item));
    item = xrtValueString(XRT_STR_LITERAL("key"));
    CHECK(item != NULL && xrtValueArrayAppendTake(args, &item));
    item = xrtValueString(XRT_STR_LITERAL("value"));
    CHECK(item != NULL && xrtValueArrayAppendTake(args, &item));
    for (failure = 0u; failure < 128u && succeeded < 2u; ++failure) {
        xbuffer* output;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        output = (xbuffer*)xr_encode_bytes(args);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(output == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            CHECK(output != NULL && xrtGetError() == NULL);
            ++succeeded;
        }
        xrtBufferDestroy(output);
    }
    CHECK(rejected >= 3u && succeeded == 2u);

    rejected = succeeded = 0u;
    for (failure = 0u; failure < 128u && succeeded < 2u; ++failure) {
        xvalue* output;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        output = xr_encode(args);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(output != NULL && xrtValueType(output) == XVALUE_NULL &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            xstrview text;
            CHECK(output != NULL && xrtGetError() == NULL &&
                xrtValueGetString(output, &text) &&
                text.Size == sizeof(expected) - 1u &&
                memcmp(text.Data, expected, text.Size) == 0);
            ++succeeded;
        }
        xrtValueRelease(output);
    }
    CHECK(rejected >= 3u && succeeded == 2u);
    xrtValueRelease(args);
    return 0;
}

static int test_decode(void)
{
    static const char reply[] = "*3\r\n+OK\r\n$3\r\na\0b\r\n*2\r\n:1\r\n$-1\r\n";
    unsigned failure, rejected = 0u, succeeded = 0u;
    for (failure = 0u; failure < 128u && succeeded < 2u; ++failure) {
        xvalue* output;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        output = xr_decode_raw((void*)reply, sizeof(reply) - 1u);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(output != NULL && xrtValueType(output) == XVALUE_NULL &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            CHECK(output != NULL && xrtValueType(output) == XVALUE_OBJECT &&
                xrtGetError() == NULL);
            ++succeeded;
        }
        xrtValueRelease(output);
    }
    CHECK(rejected >= 10u && succeeded == 2u);
    return 0;
}

int main(void)
{
    CHECK(test_encode() == 0);
    CHECK(test_decode() == 0);
    puts("redis allocation: PASS");
    return 0;
}
