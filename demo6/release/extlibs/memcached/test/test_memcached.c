#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/memcached_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "memcached check failed at line %d\n", __LINE__); \
    return 1; \
} } while (0)

static xvalue *field(xvalue *object, const char *name)
{
    return xrtValueObjectGet(object, (xstrview){name, strlen(name)});
}

int main(void)
{
    static const uint8 response[] =
        "VALUE k 0 3\r\na\0b\r\nEND\r\n";
    static const uint8 multi[] =
        "VALUE a 3 1\r\nX\r\nVALUE b 0 0\r\n\r\nEND\r\n";
    static const uint8 binary[] =
        "VALUE bin 0 2\r\n\xff\0\r\nEND\r\n";
    xvalue *keys = xrtValueArray();
    xvalue *parsed;
    xvalue *items;
    xstrview data;
    char *command;
    CHECK(keys != NULL);
    CHECK(xrtValueArrayAppendNew(keys,
        xrtValueString((xstrview){"a", 1})));
    CHECK(xrtValueArrayAppendNew(keys,
        xrtValueString((xstrview){"b", 1})));
    command = xmc_build_get(keys);
    CHECK(command != NULL && strcmp(command, "get a b\r\n") == 0);
    xrtFree(command);
    xrtValueRelease(keys);

    command = xmc_build_set((xstrview){"key", 3}, 1, 0, 3);
    CHECK(command != NULL && strcmp(command, "set key 1 0 3\r\n") == 0);
    xrtFree(command);
    command = xmc_build_set((xstrview){"a b", 3}, 0, 0, 0);
    CHECK(command == NULL);
    command = xmc_build_delete((xstrview){"k\r\nflush_all", 12});
    CHECK(command == NULL);
    command = xmc_build_add((xstrview){"k", 1}, 0, 0, -1);
    CHECK(command == NULL);

    CHECK(xmc_frame_size((xbytesview){response, 15}) == 0);
    CHECK(xmc_frame_size((xbytesview){response, sizeof(response) - 1}) ==
        (int64)(sizeof(response) - 1));
    parsed = xmc_parse_response_span((xbytesview){response,
        sizeof(response) - 1});
    CHECK(parsed != NULL);
    items = field(parsed, "items");
    CHECK(items != NULL && xrtValueCount(items) == 1);
    CHECK(xrtValueGetString(field(xrtValueArrayAt(items, 0), "data"), &data));
    CHECK(data.Size == 3 && memcmp(data.Data, "a\0b", 3) == 0);
    xrtValueRelease(parsed);

    parsed = xmc_parse_response_span((xbytesview){binary,
        sizeof(binary) - 1});
    CHECK(parsed != NULL && xrtValueType(field(
        xrtValueArrayAt(field(parsed, "items"), 0), "data")) == XVALUE_BYTES);
    xrtValueRelease(parsed);

    CHECK(xmc_frame_size((xbytesview){multi, sizeof(multi) - 1}) ==
        (int64)(sizeof(multi) - 1));
    parsed = xmc_parse_response_span((xbytesview){multi, sizeof(multi) - 1});
    CHECK(parsed != NULL && xrtValueCount(field(parsed, "items")) == 2);
    xrtValueRelease(parsed);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"END\r\n", 5}) == 5);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"VALUE k 0 x\r\n", 13}) < 0);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"VALUE k 0 3\r\nab",
        sizeof("VALUE k 0 3\r\nab") - 1}) == 0);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"UNKNOWN\r\n", 9}) < 0);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"STORED\r\nEND\r\n", 13}) == 8);
    CHECK(xmc_frame_size((xbytesview){NULL, 0}) == 0);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"STAT pid 1\r\nEND\r\n",
        sizeof("STAT pid 1\r\nEND\r\n") - 1}) ==
        (int64)(sizeof("STAT pid 1\r\nEND\r\n") - 1));
    parsed = xmc_parse_response_span((xbytesview){
        (const uint8 *)"STAT pid 1\r\nEND\r\n",
        sizeof("STAT pid 1\r\nEND\r\n") - 1});
    CHECK(parsed != NULL && xrtValueGetString(
        field(field(parsed, "stats"), "pid"), &data) &&
        data.Size == 1 && data.Data[0] == '1');
    xrtValueRelease(parsed);
    CHECK(xmc_frame_size((xbytesview){(const uint8 *)"VALUE k 0 999999999\r\n",
        sizeof("VALUE k 0 999999999\r\n") - 1}) < 0);
    {
        static const uint8 cas[] =
            "VALUE k 0 1 18446744073709551615\r\nX\r\nEND\r\n";
        parsed = xmc_parse_response_span((xbytesview){cas, sizeof(cas) - 1});
        CHECK(parsed != NULL && xrtValueGetString(field(
            xrtValueArrayAt(field(parsed, "items"), 0), "cas"), &data) &&
            data.Size == 20 &&
            memcmp(data.Data, "18446744073709551615", 20) == 0);
        xrtValueRelease(parsed);
    }
    {
        unsigned failure;
        for (failure = 0; failure < 48; ++failure) {
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            parsed = xmc_parse_response_span((xbytesview){multi,
                sizeof(multi) - 1});
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) CHECK(parsed == NULL ||
                xrtValueType(parsed) == XVALUE_NULL);
            else CHECK(parsed != NULL &&
                xrtValueType(parsed) == XVALUE_OBJECT);
            xrtValueRelease(parsed);
        }
    }
    puts("memcached C protocol: ok");
    return 0;
}
