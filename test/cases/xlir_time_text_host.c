#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define VIEW(Text) ((xstrview){Text, sizeof(Text)-1})
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}
static void exact(xstrview text, xstrview expected)
{
    assert(text.Data && text.Size == expected.Size && !memcmp(text.Data, expected.Data, text.Size));
    assert(text.Data[text.Size] == 0); xrtFree((void*)text.Data);
}
static void functional(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xtime value = INT64_C(1709251137123);
    exact(xl6TimeFormatV1(value, VIEW("%F %T.%f %:z"), 28800, -1), VIEW("2024-03-01 07:58:57.123 +08:00"));
    exact(xl6TimeFormatV1(value, VIEW("%F\0junk"), 0, 2), VIEW("2024-02-29"));
    exact(xl6TimeFormatV1(value, VIEW(""), 0, -1), VIEW(""));
    exact(xl6TimeRFC3339V1(value, -18000), VIEW("2024-02-29T18:58:57.123-05:00"));
    exact(xl6TimeHTTPDateV1(INT64_C(784111777000000)), VIEW("Sun, 06 Nov 1994 08:49:37 GMT"));
    assert(xl6TimeParseAnyV1(VIEW("2024-02-29 23:58:57\0junk"), 19) == value-123);
    assert(xl6TimeParsePatternV1(VIEW("2024/2/29 23:58:57"), VIEW("%Y/%-m/%-d %T"), 999, 999) == value-123);
    assert(xl6TimeParseRFC3339V1(VIEW("2024-02-29T23:58:57.123789Z"), -1) == value);
    assert(xl6TimeParseHTTPDateV1(VIEW("Sunday, 06-Nov-94 08:49:37 GMT"), -1) == INT64_C(784111777000000));
    void* fields = xl6DateTimeParseV1(VIEW("2024-03-01 07:58:57.123 +08:00"), VIEW("%F %T.%f %:z"), -1, -1);
    assert(fields && xl6DateTimeToTimeV1(fields) == value && xl6DateTimeOffsetV1(fields) == 28800);
    exact(xl6DateTimeFormatV1(fields, VIEW("%F %T.%f %:z"), -1), VIEW("2024-03-01 07:58:57.123 +08:00"));
    xl6DateTimeDestroyV1(fields);
    assert(!xrtGetError()); balanced(before);
    /* HTTP try is allocation-free; all try functions restore prior errors on
     * success/input mismatch, but never conceal diagnostic allocation failure. */
    xrtSetErrorInfo(XERR_STATE, "time-test", 9, "pre-existing");
    const xerror* pending = xrtGetError(); assert(pending);
    xrtMemDebugSnapshot(&before);
    XL6_ProductBoolTimeV1 out = xl6TimeTryParseHTTPDateV1(VIEW("bad"), -1);
    assert(!out.e0 && out.e1 == 0 && xrtGetError() == pending);
    out = xl6TimeTryParseHTTPDateV1(VIEW("Sun, 06 Nov 1994 08:49:37 GMT"), -1);
    assert(out.e0 && out.e1 == INT64_C(784111777000000) && xrtGetError() == pending);
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after); assert(after.AllocCount == before.AllocCount);
    out = xl6TimeTryParseAnyV1(VIEW("bad"), -1);
    assert(!out.e0 && !out.e1 && xrtGetError() == pending);
    out = xl6TimeTryParseAnyV1(VIEW("2024-02-29"), -1);
    assert(out.e0 && xrtGetError() == pending);
    out = xl6TimeTryParsePatternV1(VIEW("bad"), VIEW("%F"), -1, -1);
    assert(!out.e0 && !out.e1 && xrtGetError() == pending);
    out = xl6TimeTryParseRFC3339V1(VIEW("bad"), -1);
    assert(!out.e0 && !out.e1 && xrtGetError() == pending);
    out = xl6TimeTryParseRFC3339V1(VIEW("2024-02-29T23:58:57.123Z"), -1);
    assert(out.e0 && out.e1 == value && xrtGetError() == pending);
    balanced(before); xrtClearError();
    (void)xl6TimeParseAnyV1(VIEW("2024-02-29\0junk"), -1);
    assert(xrtErrorFind(xrtGetError(), "xrt.time", XTIME_ERROR_PARSE)); xrtClearError();
    (void)xl6TimeFormatV1(value, VIEW("%F\0junk"), 0, -1);
    assert(xrtErrorFind(xrtGetError(), "xrt.time", XTIME_ERROR_FORMAT)); xrtClearError();
    (void)xl6TimeTryParsePatternV1(VIEW("2024-02-29"), VIEW("%Q"), -1, -1);
    assert(xrtErrorFind(xrtGetError(), "xrt.time", XTIME_ERROR_FORMAT)); xrtClearError();
    (void)xl6TimeFormatV1(value, VIEW("\xe4\xbd\xa0"), 0, 1);
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
    (void)xl6TimeParseAnyV1(VIEW(""), -2);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    (void)xl6TimeParseAnyV1((xstrview){NULL, 1}, -1);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    (void)xl6DateTimeFormatV1(NULL, VIEW("%F"), -1);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    (void)xl6TimeRFC3339V1(value, INT64_MAX);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    (void)xl6TimeRFC3339V1(value, 1);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
}
static void failurePath(unsigned kind)
{
    xstrview text = {NULL, 0};
    switch (kind) {
    case 0: text = xl6TimeFormatV1(0, VIEW("%F"), 0, -1); break;
    case 1: text = xl6TimeRFC3339V1(0, 0); break;
    case 2: text = xl6TimeHTTPDateV1(0); break;
    case 3: text = xl6TimeFormatV1(0, VIEW("%Q"), 0, -1); break;
    case 4: (void)xl6TimeParseAnyV1(VIEW("bad"), -1); break;
    case 5: (void)xl6TimeTryParseAnyV1(VIEW("bad"), -1); break;
    case 6: (void)xl6TimeTryParsePatternV1(VIEW("bad"), VIEW("%F"), -1, -1); break;
    case 7: (void)xl6TimeTryParseRFC3339V1(VIEW("bad"), -1); break;
    case 8: (void)xl6TimeTryParsePatternV1(VIEW("bad"), VIEW("%Q"), -1, -1); break;
    case 9: xl6DateTimeDestroyV1(xl6DateTimeParseV1(VIEW("2024-02-29"), VIEW("%F"), -1, -1)); break;
    case 10: xl6DateTimeDestroyV1(xl6DateTimeParseV1(VIEW("bad"), VIEW("%F"), -1, -1)); break;
    default: assert(0);
    }
    xrtFree((void*)text.Data);
}
static void failures(void)
{
    unsigned failures = 0, completions = 0;
    for (unsigned kind=0; kind<11; ++kind) {
        bool completed = false;
        for (unsigned point=0; point<100; ++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point)); failurePath(kind);
            bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (hit) { assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failures; }
            else { ++completions; completed = true; }
            xrtClearError(); balanced(before);
            if (!hit) break;
        }
        assert(completed);
    }
    assert(failures >= 11 && completions == 11);
    printf("time text errors: %u actual allocation failures, %u first no-hit completions, balanced\n", failures, completions);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); functional(); failures();
    puts("time text native: exact lengths, protocol vectors, pending errors, one-pass try and ownership passed");
    return 0;
}
