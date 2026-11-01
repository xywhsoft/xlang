/* 独立小端 BSON Date 线格式，检查 AD 毫秒与 Unix 毫秒的纪元差。 */
#include "../bson_native.c"
#include <inttypes.h>
#include <stdio.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "bson date epochs:%d: %s\n", __LINE__, #e); return 1; } } while (0)

static const int64_t epoch = INT64_C(62135596800000);

int main(void)
{
    static const int64_t cases[] = {
        INT64_MIN, -INT64_C(62135596800001), -INT64_C(62135596800000),
        -1000, -500, -1, 0, 1, 500, 1000, INT64_C(253402300799999),
        INT64_MAX - INT64_C(62135596800000),
        INT64_MAX - INT64_C(62135596800000) + 1, INT64_MAX
    };
    unsigned paths = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8 wire[19] = {19, 0, 0, 0, 9, 'w', 'h', 'e', 'n', 0};
        uint64_t bits = (uint64_t)cases[i];
        for (unsigned b = 0; b < 8; ++b) wire[10+b] = (uint8)(bits >> (8*b));
        bool valid = cases[i] <= INT64_MAX - epoch;
        int64_t expected = valid ? cases[i] + epoch : 0;
        xvalue* source = xrtValueBytes((xbytesview){wire, sizeof(wire)});
        char* hex = xbson_to_hex(source);
        CHECK(source && hex);
        for (unsigned api = 0; api < 4; ++api) {
            xvalue* result = api == 0 ? xbson_decode(source) :
                api == 1 ? xbson_decode_span((xbytesview){wire, sizeof(wire)}) :
                api == 2 ? xbson_decode_all(source) :
                xbson_decode_all_span((xbytesview){wire, sizeof(wire)});
            xtime observed = 0;
            if (valid) {
                CHECK(result && !xrtGetError());
                xvalue* document = api < 2 ? result : xrtValueArrayGet(result, 0);
                CHECK(document && xrtValueGetTime(xrtValueObjectGet(document,
                    XRT_STR_LITERAL("when")), &observed) && observed == expected);
                xvalue* encoded = xbson_encode(document);
                xbytesview output = {0};
                CHECK(encoded && !xrtGetError() && xrtValueGetBytes(encoded, &output) &&
                    output.Size == sizeof(wire) && !memcmp(output.Data, wire, sizeof(wire)));
                xrtValueRelease(encoded);
            } else {
                CHECK(result && xrtValueType(result) == XVALUE_NULL &&
                    xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
            }
            printf("date\t%u\t%" PRId64 "\t%" PRId64 "\t%s\n",
                api, cases[i], (int64_t)observed, hex);
            xrtValueRelease(result);
            ++paths;
        }
        xrtFree(hex);
        xrtValueRelease(source);
    }

    // XRT 原始时间可表示完整 int64；只在纪元换算溢出时拒绝，不能任意限制年份。
    for (unsigned i = 0; i < 2; ++i) {
        xrtClearError();
        xvalue* document = xrtValueObject();
        xvalue* value = xrtValueTime(i ? INT64_MIN + epoch - 1 : INT64_MIN);
        CHECK(document && value && xrtValueObjectSetTake(document, XRT_STR_LITERAL("when"), &value));
        xvalue* result = xbson_encode(document);
        CHECK(result && xrtValueType(result) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(result);
        xrtValueRelease(document);
    }
    printf("bson date epochs: %u decode paths, 2 encode rejections, PASS\n", paths);
    return 0;
}
