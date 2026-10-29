#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
}
static void functional(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    assert(xl6TimeFromDateV1(1970, 1, 1) == XRT_TIME_UNIX_EPOCH);
    assert(xl6TimeFromDateV1(2000, 2, 29) == INT64_C(63087379200000));
    assert(xl6TimeFromDateTimeV1(1970, 1, 1, 0, 0, 0, 1, 1) == INT64_C(62135596799001));
    assert(xl6TimeFromTimeV1(0, 0, 0, 1) == XRT_TIME_UNIX_EPOCH + 1);
    assert(xl6TimeFromTimeV1(23, 59, 59, 999) == (XRT_TIME_UNIX_EPOCH + INT64_C(86399999)));
    assert(xl6TimeFromUnixV1(-1) == XRT_TIME_UNIX_EPOCH - 1000);
    assert(xl6TimeFromUnixMsV1(-1) == XRT_TIME_UNIX_EPOCH - 1);
    assert(xl6TimeUnixV1(XRT_TIME_UNIX_EPOCH - 1) == -1 && xl6TimeUnixMsV1(XRT_TIME_UNIX_EPOCH - 1) == -1);
    assert(xl6TimeUnixV1(INT64_MIN) == INT64_C(-9223434172451576));
    assert(xl6TimeMillisecondV1(-1) == 999);
    assert(xl6TimeNearV1(INT64_MIN, INT64_MAX, UINT64_MAX));
    assert(!xl6TimeNearV1(INT64_MIN, INT64_MAX, UINT64_MAX - 1));
    assert(!xl6TimeNearV1(0, 1, 0) && xl6TimeNearV1(0, 1, 1));
    int64_t january = xl6TimeFromDateV1(2024, 1, 31);
    assert(january == INT64_C(63842256000000));
    assert(xl6TimeAddV1(january, 1, XTIME_UNIT_MONTH) == INT64_C(63844761600000));
    assert(xl6TimeAddV1(january, -1, XTIME_UNIT_YEAR) == xl6TimeFromDateV1(2023, 1, 31));
    assert(xl6TimeAddV1(XRT_TIME_UNIX_EPOCH, 1, XTIME_UNIT_QUARTER) == xl6TimeFromDateV1(1970, 4, 1));
    assert(xl6TimeDiffV1(1, -1, XTIME_UNIT_MILLISECOND) == -2);
    assert(xl6TimeDiffV1(0, 1999, XTIME_UNIT_MILLISECOND) == 1999);
    assert(xl6TimeDiffV1(0, -1999, XTIME_UNIT_MILLISECOND) == -1999);
    uint64_t clock = xl6TimeMonotonicV1();
    assert(xl6TimeMonotonicV1() >= clock);
    assert(xl6TimeNowV1() > 0);
    xl6TimeSleepV1(0);
    xl6TimeSleepUntilV1(0);
    assert(!xrtGetError());
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount == after.AllocCount); /* no successful-path heap work */
    balanced(before);
    xrtSetErrorKind(XERR_STATE);
    const xerror* pending = xrtGetError();
    assert(xl6TimeUnixV1(XRT_TIME_UNIX_EPOCH - 1) == -1 && xl6TimeUnixMsV1(XRT_TIME_UNIX_EPOCH - 1) == -1);
    assert(xl6TimeMillisecondV1(-1) == 999);
    assert(xl6TimeNearV1(INT64_MIN, INT64_MAX, UINT64_MAX));
    assert(xrtGetError() == pending); /* no-raise queries neither consume nor replace errors */
    xrtClearError();
    balanced(before);
}
static void invalid(unsigned kind)
{
    switch (kind) {
    case 0: (void)xl6TimeFromDateV1(1970, INT64_C(4294967297), 1); break;
    case 1: (void)xl6TimeFromDateV1(2023, 2, 29); break;
    case 2: (void)xl6TimeFromUnixV1(INT64_MAX); break;
    case 3: (void)xl6TimeFromUnixMsV1(INT64_MAX); break;
    case 4: (void)xl6TimeAddV1(INT64_MAX, 1, XTIME_UNIT_MILLISECOND); break;
    case 5: (void)xl6TimeDiffV1(INT64_MIN, INT64_MAX, XTIME_UNIT_MILLISECOND); break;
    case 6: (void)xl6TimeAddV1(0, 0, INT64_MAX); break;
    case 7: (void)xl6TimeFromDateTimeV1(1970, 1, 1, 0, 0, 0, 0, INT64_MIN); break;
    default: assert(0);
    }
}
static void failures(void)
{
    unsigned failures = 0, completions = 0;
    for (unsigned kind = 0; kind < 8; ++kind) {
        bool completed = false;
        for (unsigned point = 0; point < 100; ++point) {
            xmemdebugsnapshot before;
            xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            invalid(kind);
            bool hit = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            assert(xrtGetError());
            if (hit) { assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failures; }
            else { assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); ++completions; completed = true; }
            xrtClearError();
            balanced(before);
            if (!hit) break;
        }
        assert(completed);
    }
    printf("time adapter errors: %u actual allocation failures, %u first no-hit completions, balanced\n", failures, completions);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    functional();
    failures();
    puts("time adapter: full-width components, milliseconds, offset, overflow, monotonic and zero-allocation success passed");
    return 0;
}
