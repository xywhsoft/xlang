/* 截断字段不能先制造零值再释放，更不能用该分配失败覆盖已知格式错误。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_TIME
#define XRT_MODULE_UNICODE
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_MEMORY_DEBUG
#include "../bson_native.h"
#include <stdio.h>
#include <string.h>

static unsigned factories, after_error, masked, vectors, failures, oom_paths;
static bool force_factory_failure;

/* 仅观察生产代码调用公开值工厂；故障只注入到格式已经失败后的错误分配。 */
static bool scalar_begin(void)
{
    ++factories;
    bool bad = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    if (bad) ++after_error;
    return bad && force_factory_failure && xrtMemDebugFailAfter(0);
}

static xvalue* scalar_end(xvalue* value, bool injected)
{
    if (injected) {
        if (xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError()) == XERR_MEMORY) ++masked;
        xrtMemDebugFailClear();
    }
    return value;
}

static xvalue* observe_int(int64 n) { bool fail = scalar_begin(); return scalar_end(xrtValueInt(n), fail); }
static xvalue* observe_float(double n) { bool fail = scalar_begin(); return scalar_end(xrtValueFloat(n), fail); }
static xvalue* observe_time(xtime n) { bool fail = scalar_begin(); return scalar_end(xrtValueTime(n), fail); }

#define xrtValueInt observe_int
#define xrtValueFloat observe_float
#define xrtValueTime observe_time
#include "../bson_native.c"
#undef xrtValueInt
#undef xrtValueFloat
#undef xrtValueTime

static const struct { uint8 type; unsigned size; uint64 bits; } numbers[] = {
    {1, 8, UINT64_C(0x3ff4000000000000)}, /* 1.25 */
    {16, 4, INT32_MAX},
    {18, 8, UINT64_C(0x8000000000000000)},
    {9, 8, UINT64_MAX - 499} /* -500 毫秒 */
};

static void le32(uint8* data, size_t n)
{
    for (unsigned i = 0; i != 4; ++i) data[i] = (uint8)(n >> (8*i));
}

static size_t document(uint8* data, unsigned kind, unsigned bytes, unsigned context)
{
    uint8 inner[16];
    size_t size = bytes + 8;
    le32(inner, size);
    inner[4] = numbers[kind].type;
    inner[5] = context == 2 ? '0' : 'x'; inner[6] = 0;
    for (unsigned i = 0; i != bytes; ++i) inner[7+i] = (uint8)(numbers[kind].bits >> (8*i));
    inner[size-1] = 0;
    if (context == 0) { memcpy(data, inner, size); return size; }
    le32(data, size + 8); data[4] = context == 1 ? 3 : 4;
    data[5] = 'n'; data[6] = 0;
    memcpy(data + 7, inner, size); data[size+7] = 0;
    return size + 8;
}

static xvalue* decode(const uint8* data, size_t size, xvalue* source, unsigned api)
{
    if (api == 0) return xbson_decode_span((xbytesview){data, size});
    if (api == 1) return xbson_decode(source);
    return (api & 1) ? xbson_decode_all(source) : xbson_decode_all_span((xbytesview){data, size});
}

static xvalue* leaf(xvalue* value, unsigned context, unsigned api)
{
    if (api >= 2) value = xrtValueArrayGet(value, api >= 4 ? 1 : 0);
    if (context != 0) value = xrtValueObjectGet(value, XRT_STR_LITERAL("n"));
    return context == 2 ? xrtValueArrayGet(value, 0) : xrtValueObjectGet(value, XRT_STR_LITERAL("x"));
}

static bool number_ok(xvalue* value, unsigned kind)
{
    int64 integer; double real; xtime time;
    if (kind == 0) return xrtValueGetFloat(value, &real) && real == 1.25;
    if (kind == 3) return xrtValueGetTime(value, &time) && time == -500000;
    return xrtValueGetInt(value, &integer) && integer == (kind == 1 ? INT32_MAX : INT64_MIN);
}

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount;
}

static bool exercise(unsigned kind, unsigned bytes, unsigned context, unsigned api, bool fault, bool oom)
{
    uint8 data[40]; size_t offset = api >= 4 ? 5 : 0;
    if (offset) { le32(data, 5); data[4] = 0; }
    size_t size = offset + document(data + offset, kind, bytes, context);
    xvalue* source = xrtValueBytes((xbytesview){data, size});
    if (source == NULL) return false;
    bool complete = bytes == numbers[kind].size, ok = true, completed = false, rejected = false;
    unsigned points = oom ? 128 : 1;
    for (unsigned point = 0; point != points; ++point) {
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        factories = after_error = masked = 0;
        force_factory_failure = fault;
        if (oom && !xrtMemDebugFailAfter(point)) { ok = false; break; }
        xvalue* result = decode(data, size, source, api);
        bool triggered = oom && xrtMemDebugFailTriggered();
        if (oom) xrtMemDebugFailClear();
        force_factory_failure = false;
        if (triggered) {
            rejected = true;
            ok = result != NULL && xrtValueType(result) == XVALUE_NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY;
        } else if (complete) {
            ok = result != NULL && xrtGetError() == NULL && factories == 1 && number_ok(leaf(result, context, api), kind);
        } else {
            ok = result != NULL && xrtValueType(result) == XVALUE_NULL && factories == 0 &&
                xrtErrorKind(xrtGetError()) == XERR_VALUE;
        }
        if (!ok && failures < 6) fprintf(stderr, "numeric type=%u bytes=%u context=%u api=%u fault=%d point=%u factories=%u after_error=%u masked=%u kind=%d\n",
            numbers[kind].type, bytes, context, api, (int)fault, point, factories, after_error, masked,
            xrtGetError() != NULL ? (int)xrtErrorKind(xrtGetError()) : -1);
        xrtValueRelease(result);
        ok = balanced(before) && ok;
        if (ok && triggered) {
            factories = after_error = masked = 0;
            result = decode(data, size, source, api);
            ok = result != NULL && xrtGetError() == NULL && factories == 1 &&
                number_ok(leaf(result, context, api), kind);
            xrtValueRelease(result);
            ok = balanced(before) && ok;
        }
        if (!ok) break;
        if (!triggered) { completed = true; break; }
    }
    xrtValueRelease(source);
    return ok && completed && (!oom || rejected);
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.bson", 0, "warm slot"); xrtClearError();
    for (unsigned kind = 0; kind != 4; ++kind) for (unsigned context = 0; context != 3; ++context)
        for (unsigned api = 0; api != 6; ++api) {
            for (unsigned bytes = 0; bytes < numbers[kind].size; ++bytes)
                for (unsigned fault = 0; fault != 2; ++fault) {
                    ++vectors;
                    if (!exercise(kind, bytes, context, api, fault != 0, false)) ++failures;
                }
            ++vectors;
            if (!exercise(kind, numbers[kind].size, context, api, false, false)) ++failures;
            ++oom_paths;
            if (!exercise(kind, numbers[kind].size, context, api, false, true)) ++failures;
        }
    printf("bson numeric reads: %u vectors, %u OOM paths, %u failures\n", vectors, oom_paths, failures);
    return failures != 0;
}
