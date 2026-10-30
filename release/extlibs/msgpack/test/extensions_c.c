/* 扩展帧先校验完整长度，再区分标准时间戳、未知类型和损坏输入。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "../msgpack_native.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
static const uint8 heads[] = {0xd4,0xd5,0xd6,0xd7,0xd8,0xc7,0xc8,0xc9};
static const size_t lengths[] = {0,1,2,3,4,8,12,16,17,255,256};
static const uint8 types[] = {0,0x7f,0x80,0xfe,0xff};

static size_t frame(uint8* out, uint8 head, uint8 type, size_t size, const uint8* payload)
{
    size_t n = 0;
    out[n++] = head;
    unsigned width = head == 0xc7 ? 1 : (head == 0xc8 ? 2 : (head == 0xc9 ? 4 : 0));
    for (unsigned i = 0; i != width; ++i) out[n++] = (uint8)(size >> ((width - i - 1) * 8));
    out[n++] = type;
    if (payload != NULL) memcpy(out + n, payload, size);
    else memset(out + n, 0, size);
    return n + size;
}

static size_t wrap(uint8* out, const uint8* input, size_t size, unsigned context)
{
    static const uint8 prefixes[][3] = {{0}, {0x91}, {0x81,0xa1,'x'}};
    size_t prefix = context == 0 ? 0 : (context == 1 ? 1 : 3);
    memcpy(out, prefixes[context], prefix);
    memcpy(out + prefix, input, size);
    return prefix + size;
}

static xvalue* payload_value(xvalue* value, unsigned context)
{
    return context == 0 ? value : (context == 1 ? xrtValueArrayGet(value, 0) :
        xrtValueObjectGet(value, XRT_STR_LITERAL("x")));
}

static void verify(const uint8* data, size_t size, unsigned context, bool abi, xerrkind kind)
{
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xvalue* value = abi ? xmsgpack_decode_abi((void*)data, size) : xmsgpack_decode(data, size);
    const xerror* error = xrtGetError();
    bool ok;
    if (kind == XERR_NONE) {
        xtime time;
        ok = value != NULL && error == NULL &&
            xrtValueGetTime(payload_value(value, context), &time) && time == 0;
    } else {
        ok = (abi ? value != NULL && xrtValueType(value) == XVALUE_NULL : value == NULL) &&
            error != NULL && xrtErrorKind(error) == kind &&
            strcmp(xrtErrorDomain(error), "xlang.extlib.msgpack") == 0;
    }
    if (!ok && failures < 8) fprintf(stderr, "extension head=%02x size=%zu context=%u abi=%d expected=%d actual=%d\n",
        size != 0 ? data[0] : 0, size, context, (int)abi, (int)kind,
        error != NULL ? (int)xrtErrorKind(error) : -1);
    xrtValueRelease(value);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    ok = ok && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount;
    ++checks;
    if (!ok) ++failures;
}

static void matrix(void)
{
    uint8 data[512], wrapped[516];
    for (unsigned h = 0; h != sizeof(heads); ++h) {
        for (size_t l = 0; l != sizeof(lengths) / sizeof(lengths[0]); ++l) {
            size_t size = lengths[l];
            if (h < 5 && size != ((size_t)1 << h)) continue;
            if (heads[h] == 0xc7 && size > 255) continue;
            for (unsigned t = 0; t != sizeof(types); ++t) {
                size_t count = frame(data, heads[h], types[t], size, NULL);
                bool timestamp = types[t] == 0xff && (size == 4 || size == 8 || size == 12);
                xerrkind kind = types[t] != 0xff ? XERR_UNSUPPORTED : (timestamp ? XERR_NONE : XERR_VALUE);
                for (unsigned context = 0; context != 3; ++context) {
                    for (unsigned abi = 0; abi != 2; ++abi) {
                        size_t total = wrap(wrapped, data, count, context);
                        verify(wrapped, total, context, abi != 0, kind);
                        for (size_t cut = 0; cut != count; ++cut) {
                            total = wrap(wrapped, data, cut, context);
                            verify(wrapped, total, context, abi != 0,
                                total == 0 ? XERR_ARGUMENT : XERR_VALUE);
                        }
                    }
                }
            }
        }
    }
    data[0] = 0xc1;
    for (unsigned context = 0; context != 3; ++context)
        for (unsigned abi = 0; abi != 2; ++abi)
            verify(wrapped, wrap(wrapped, data, 1, context), context, abi != 0, XERR_VALUE);
    data[0] = 0x81; data[1] = 0xc1; data[2] = 0;
    verify(data, 3, 0, false, XERR_VALUE);
    verify(data, 3, 0, true, XERR_VALUE);
    /* ext32 的长度本身超过公开输入预算，无需等分配失败才诊断。 */
    static const uint8 oversized[] = {0xc9,0x04,0,0,1,0};
    verify(oversized, sizeof(oversized), 0, false, XERR_RANGE);
    verify(oversized, sizeof(oversized), 0, true, XERR_RANGE);
}

/* 非整微秒、非法纳秒和溢出在所有可容纳该载荷的头部中含义相同。 */
static void invalid_times(void)
{
    static const struct { size_t size; uint8 payload[12]; xerrkind kind; } cases[] = {
        {8, {0,0,0,4,0,0,0,0}, XERR_VALUE},
        {8, {0xee,0x6b,0x28,0,0,0,0,0}, XERR_VALUE},
        {12, {0,0,0,1,0,0,0,0,0,0,0,0}, XERR_VALUE},
        {12, {0x3b,0x9a,0xca,0,0,0,0,0,0,0,0,0}, XERR_VALUE},
        {12, {0,0,0,0,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff}, XERR_RANGE},
        {12, {0,0,0,0,0x80,0,0,0,0,0,0,0}, XERR_RANGE}
    };
    uint8 data[32], wrapped[36];
    for (size_t c = 0; c != sizeof(cases)/sizeof(cases[0]); ++c)
        for (unsigned h = 3; h != sizeof(heads); ++h) {
            if (h < 5 && cases[c].size != ((size_t)1 << h)) continue;
            size_t size = frame(data, heads[h], 0xff, cases[c].size, cases[c].payload);
            for (unsigned context = 0; context != 3; ++context)
                for (unsigned abi = 0; abi != 2; ++abi)
                    verify(wrapped, wrap(wrapped, data, size, context), context, abi != 0, cases[c].kind);
        }
}

static bool oom(void)
{
    uint8 data[32], wrapped[36];
    for (unsigned h = 5; h != 8; ++h) for (unsigned context = 0; context != 3; ++context)
        for (unsigned abi = 0; abi != 2; ++abi) {
            size_t size = frame(data, heads[h], 0xff, 12, NULL);
            size = wrap(wrapped, data, size, context);
            bool completed = false, failed = false;
            for (uint64 budget = 0; budget != 128; ++budget) {
                xmemdebugsnapshot before, after;
                xrtClearError(); xrtMemDebugSnapshot(&before);
                if (!xrtMemDebugFailAfter(budget)) return false;
                xvalue* value = abi ? xmsgpack_decode_abi(wrapped, size) : xmsgpack_decode(wrapped, size);
                bool triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                xtime time;
                bool ok = triggered ? (abi ? xrtValueType(value) == XVALUE_NULL : value == NULL) &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY : value != NULL && xrtGetError() == NULL &&
                    xrtValueGetTime(payload_value(value, context), &time) && time == 0;
                failed = failed || triggered;
                xrtValueRelease(value); xrtClearError(); xrtMemDebugSnapshot(&after);
                if (!ok || before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
                    before.DoubleFreeCount != after.DoubleFreeCount || before.InvalidFreeCount != after.InvalidFreeCount) return false;
                if (triggered) {
                    value = xmsgpack_decode(wrapped, size);
                    ok = value != NULL && xrtGetError() == NULL &&
                        xrtValueGetTime(payload_value(value, context), &time) && time == 0;
                    xrtValueRelease(value); xrtClearError(); xrtMemDebugSnapshot(&after);
                    if (!ok || before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) return false;
                }
                if (!triggered) { completed = true; break; }
            }
            if (!completed || !failed) return false;
        }
    return true;
}

static void hex(const uint8* data, size_t size)
{
    for (size_t i = 0; i != size; ++i) printf("%02x", data[i]);
}

static bool time_records(void)
{
    static const xtime fixed[] = {INT64_MIN,INT64_MIN+1,-1000001,-1000000,-999999,-1,
        0,1,999999,1000000,1000001,INT64_C(4294967295)*1000000,
        INT64_C(4294967296)*1000000-1,INT64_C(4294967296)*1000000,
        INT64_C(17179869184)*1000000-1,INT64_C(17179869184)*1000000,INT64_MAX-1,INT64_MAX};
    uint64 seed = UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned i = 0; i != sizeof(fixed)/sizeof(fixed[0])+512; ++i) {
        xtime time;
        if (i < sizeof(fixed)/sizeof(fixed[0])) time = fixed[i];
        else {
            seed = seed * UINT64_C(6364136223846793005) + 1;
            time = seed <= INT64_MAX ? (int64)seed : -1 - (int64)(UINT64_MAX - seed);
        }
        xvalue* source = xrtValueTime(time);
        xbuffer* canonical = xmsgpack_encode(source);
        xrtValueRelease(source);
        if (canonical == NULL) return false;
        size_t offset = canonical->Data[0] == 0xc7 ? 3 : 2;
        size_t length = canonical->Size - offset;
        uint8 encodings[] = {0xc7,0xc8,0xc9,canonical->Data[0]};
        unsigned formats = canonical->Data[0] == 0xc7 ? 3 : 4;
        for (unsigned j = 0; j != formats; ++j) {
            uint8 wire[32];
            size_t size = frame(wire, encodings[j], 0xff, length, canonical->Data + offset);
            xvalue* decoded = xmsgpack_decode(wire, size);
            xtime actual;
            printf("%u\t%lld\t%02x\t", i, (long long)time, encodings[j]); hex(wire, size);
            if (decoded != NULL && xrtValueGetTime(decoded, &actual)) printf("\ttime:%lld\t", (long long)actual);
            else printf("\terror:%d\t", xrtGetError() != NULL ? (int)xrtErrorKind(xrtGetError()) : -1);
            hex(canonical->Data, canonical->Size); putchar('\n');
            xrtValueRelease(decoded); xrtClearError();
        }
        xrtBufferDestroy(canonical);
    }
    return !ferror(stdout);
}

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--records") != 0)) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.msgpack", 0, "warm error slot"); xrtClearError();
    if (argc == 2) return time_records() ? 0 : 2;
    if (!xrtMemDebugEnabled()) return 2;
    matrix();
    invalid_times();
    if (!oom()) { ++checks; ++failures; }
    else ++checks;
    printf("msgpack extension framing: %u checks, 18 OOM paths, %u failures\n", checks, failures);
    return failures != 0;
}

#include "../msgpack_native.c"
