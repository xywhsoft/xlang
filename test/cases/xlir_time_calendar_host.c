#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
static void calendar(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    int64_t value = xl6TimeFromDateTimeV1(2024, 2, 29, 23, 59, 59, 123, 0);
    assert(xl6TimeYearV1(value) == 2024 && xl6TimeMonthV1(value) == 2 && xl6TimeDayV1(value) == 29);
    assert(xl6TimeHourV1(value) == 23 && xl6TimeMinuteV1(value) == 59 && xl6TimeSecondV1(value) == 59);
    assert(xl6TimeWeekdayV1(value) == 4 && xl6TimeDayOfYearV1(value) == 60 && xl6TimeQuarterV1(value) == 1);
    assert(xl6TimeLeapV1(-401) && !xl6TimeLeapV1(1900));
    assert(xl6TimeDaysInYearV1(2000) == 366 && xl6TimeDaysInMonthV1(2024, 2) == 29);
    assert(xl6TimeTimePartV1(-1) == XRT_TIME_DAY - 1);
    assert(xl6TimeSameDayV1(-1, -2) && !xl6TimeSameDayV1(-1, 0));
    assert(xl6TimeSameMonthV1(value, xl6TimeFromDateV1(2024, 2, 1)));
    assert(xl6TimeSameYearV1(value, xl6TimeFromDateV1(2024, 1, 1)));
    assert(xl6TimeInV1(INT64_MAX, INT64_MIN, INT64_MAX));
    assert(!xl6TimeInV1(0, INT64_MAX, INT64_MIN));
    assert(xl6TimeOverlapV1(-1, 0, 0, 1) && !xl6TimeOverlapV1(0, -1, -1, 1));
    XL6_ProductTimeX2V1 month = xl6TimeMonthRangeV1(value), year = xl6TimeYearRangeV1(value);
    XL6_ProductTimeX2V1 week = xl6TimeWeekRangeV1(value, 1);
    assert(month.e0 == xl6TimeFromDateV1(2024, 2, 1) && month.e1 == xl6TimeFromDateV1(2024, 3, 1));
    assert(year.e0 == xl6TimeFromDateV1(2024, 1, 1) && year.e1 == xl6TimeFromDateV1(2025, 1, 1));
    assert(week.e0 == xl6TimeFromDateV1(2024, 2, 26) && week.e1 == xl6TimeFromDateV1(2024, 3, 4));
    XL6_ProductI64x3V1 iso = xl6TimeISOWeekV1(xl6TimeFromDateV1(2021, 1, 1));
    assert(iso.e0 == 2020 && iso.e1 == 53 && iso.e2 == 5);
    assert(xl6TimeTimerV1() >= 0);
    xl6TimeSleepV1(0);
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount == after.AllocCount && !xrtGetError());
    balanced(before);
    xrtSetErrorKind(XERR_STATE);
    const xerror* pending = xrtGetError();
    (void)xl6TimeYearV1(INT64_MIN); (void)xl6TimeMonthV1(INT64_MAX);
    (void)xl6TimeDayV1(value); (void)xl6TimeHourV1(value); (void)xl6TimeMinuteV1(value);
    (void)xl6TimeSecondV1(value); (void)xl6TimeWeekdayV1(value); (void)xl6TimeDayOfYearV1(value);
    (void)xl6TimeQuarterV1(value); (void)xl6TimeLeapV1(2000); (void)xl6TimeDaysInYearV1(2000);
    (void)xl6TimeTimePartV1(INT64_MIN); (void)xl6TimeSameDayV1(INT64_MIN, INT64_MAX);
    (void)xl6TimeSameMonthV1(value, value); (void)xl6TimeSameYearV1(value, value);
    (void)xl6TimeInV1(value, value, value); (void)xl6TimeOverlapV1(value, value, value, value);
    assert(xrtGetError() == pending);
    xrtClearError(); balanced(before);
}
static void snapshot(void)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    void* fields = xl6DateTimeFieldsV1(2024, 2, 29, 23, 59, 58, 123, 28800);
    assert(fields && !xrtGetError());
    assert(xl6DateTimeYearV1(fields) == 2024 && xl6DateTimeMonthV1(fields) == 2 && xl6DateTimeDayV1(fields) == 29);
    assert(xl6DateTimeHourV1(fields) == 23 && xl6DateTimeMinuteV1(fields) == 59 && xl6DateTimeSecondV1(fields) == 58);
    assert(xl6DateTimeMillisecondV1(fields) == 123 && xl6DateTimeOffsetV1(fields) == 28800);
    assert(xl6DateTimeWeekdayV1(fields) == 4 && xl6DateTimeDayOfYearV1(fields) == 60 && xl6DateTimeIsDSTV1(fields) == -1);
    assert(xl6DateTimeToTimeV1(fields) == xl6TimeFromDateTimeV1(2024, 2, 29, 23, 59, 58, 123, 28800));
    xrtSetErrorKind(XERR_STATE);
    const xerror* pending = xrtGetError();
    (void)xl6DateTimeYearV1(fields); (void)xl6DateTimeMonthV1(fields); (void)xl6DateTimeDayV1(fields);
    (void)xl6DateTimeHourV1(fields); (void)xl6DateTimeMinuteV1(fields); (void)xl6DateTimeSecondV1(fields);
    (void)xl6DateTimeMillisecondV1(fields); (void)xl6DateTimeOffsetV1(fields); (void)xl6DateTimeWeekdayV1(fields);
    (void)xl6DateTimeDayOfYearV1(fields); (void)xl6DateTimeIsDSTV1(fields);
    assert(xrtGetError() == pending);
    xrtClearError(); xl6DateTimeDestroyV1(fields);
    fields = xl6DateTimeSplitV1(INT64_MIN, 86399);
    assert(fields && xl6DateTimeToTimeV1(fields) == INT64_MIN);
    xl6DateTimeDestroyV1(fields);
    fields = xl6DateTimeSplitV1(INT64_MAX, -86399);
    assert(fields && xl6DateTimeToTimeV1(fields) == INT64_MAX);
    xl6DateTimeDestroyV1(fields);
    fields = xl6DateTimeLocalV1(INT64_C(63853965059123), true);
    assert(fields && xl6DateTimeToTimeV1(fields) == INT64_C(63853965059123));
    assert(xl6DateTimeOffsetV1(fields) == xl6TimeTimezoneOffsetV1(INT64_C(63853965059123)));
    xl6DateTimeDestroyV1(fields);
    assert(!xrtGetError()); balanced(before);
}
static unsigned failureCount, completionCount;
static void failPath(unsigned kind)
{
    switch (kind) {
    case 0: xl6DateTimeDestroyV1(xl6DateTimeFieldsV1(2024, 2, 29, 0, 0, 0, 0, 0)); break;
    case 1: xl6DateTimeDestroyV1(xl6DateTimeSplitV1(0, 0)); break;
    case 2: xl6DateTimeDestroyV1(xl6DateTimeLocalV1(INT64_C(63853965059123), true)); break;
    case 3: xl6DateTimeDestroyV1(xl6DateTimeLocalV1(0, false)); break;
    case 4: xl6DateTimeDestroyV1(xl6DateTimeFieldsV1(2023, 2, 29, 0, 0, 0, 0, 0)); break;
    case 5: xl6DateTimeDestroyV1(xl6DateTimeSplitV1(0, INT64_MAX)); break;
    case 6: (void)xl6TimeDaysInMonthV1(2024, INT64_C(4294967298)); break;
    case 7: (void)xl6TimeDaysInMonthV1(2024, 0); break;
    case 8: (void)xl6TimeDatePartV1(INT64_MIN); break;
    case 9: (void)xl6TimeYearRangeV1(INT64_MAX); break;
    case 10: (void)xl6TimeWeekRangeV1(0, INT64_MAX); break;
    case 11: (void)xl6TimeWeekRangeV1(0, 7); break;
    case 12: xl6TimeSleepV1(-1); break;
    case 13: (void)xl6TimeLeapV1(0); break;
    default: assert(0);
    }
}
static void failures(void)
{
    for (unsigned kind = 0; kind < 14; ++kind) {
        bool completed = false;
        for (unsigned point = 0; point < 64; ++point) {
            xmemdebugsnapshot before;
            xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            failPath(kind);
            bool hit = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (hit) {
                if (xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                    fprintf(stderr, "calendar OOM kind=%u point=%u error=%d\n", kind, point, (int)xrtErrorKind(xrtGetError()));
                assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failureCount;
            }
            else { assert(kind < 4 ? !xrtGetError() : xrtErrorKind(xrtGetError()) == XERR_RANGE); ++completionCount; completed = true; }
            xrtClearError(); balanced(before);
            if (!hit) break;
        }
        assert(completed);
    }
}
#if !defined(_WIN32)
static void dst(const char* zone, int64_t year, int64_t month, int64_t day,
    int64_t hour, int64_t minute, int64_t earlier, int64_t later,
    int64_t gapMonth, int64_t gapDay, int64_t gapHour, int64_t gapMinute)
{
    /* Only this standalone test process is changed; never the system timezone. */
    assert(setenv("TZ", zone, 1) == 0); tzset();
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    void* fold = xl6DateTimeFieldsV1(year, month, day, hour, minute, 0, 123, 0);
    assert(fold && xl6TimeFromLocalV1(fold, 1) == earlier + 123);
    assert(xl6TimeFromLocalV1(fold, 2) == later + 123);
    void* first = xl6DateTimeLocalV1(earlier + 123, true);
    void* second = xl6DateTimeLocalV1(later + 123, true);
    assert(first && second && xl6DateTimeHourV1(first) == hour && xl6DateTimeHourV1(second) == hour);
    assert(xl6DateTimeToTimeV1(first) == earlier + 123 && xl6DateTimeToTimeV1(second) == later + 123);
    xl6DateTimeDestroyV1(first); xl6DateTimeDestroyV1(second);
    void* gap = xl6DateTimeFieldsV1(year, gapMonth, gapDay, gapHour, gapMinute, 0, 0, 0);
    assert(gap && !xrtGetError());
    for (unsigned kind = 0; kind < 3; ++kind) {
        bool completed = false;
        for (unsigned point = 0; point < 64; ++point) {
            xmemdebugsnapshot step;
            xrtMemDebugSnapshot(&step);
            assert(xrtMemDebugFailAfter(point));
            (void)xl6TimeFromLocalV1(kind == 1 ? gap : fold, kind == 2 ? INT64_MAX : 0);
            bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (hit) { assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failureCount; }
            else {
                assert(xrtErrorKind(xrtGetError()) == (kind == 2 ? XERR_RANGE : XERR_VALUE));
                assert(kind == 2 || xrtErrorCode(xrtGetError()) == (kind ? XTIME_ERROR_LOCAL_GAP : XTIME_ERROR_LOCAL_FOLD));
                ++completionCount; completed = true;
            }
            xrtClearError(); balanced(step);
            if (!hit) break;
        }
        assert(completed);
    }
    xl6DateTimeDestroyV1(fold); xl6DateTimeDestroyV1(gap); balanced(before);
}
#endif
int main(void)
{
    assert(xrtMemDebugEnable(true));
    calendar(); snapshot(); failures();
#if !defined(_WIN32)
    dst("America/New_York", 2024, 11, 3, 1, 30, INT64_C(63866208600000), INT64_C(63866212200000), 3, 10, 2, 30);
    dst("Australia/Lord_Howe", 2024, 4, 7, 1, 45, INT64_C(63848011500000), INT64_C(63848013300000), 10, 6, 2, 15);
#endif
    printf("time calendar: %u actual allocation failures, %u first no-hit completions, balanced\n", failureCount, completionCount);
    puts("calendar ranges/ISO, immutable snapshots and checked local conversion passed");
    return 0;
}
