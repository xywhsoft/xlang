#include "src/runtime/xl6_text_value.h"
#include <limits.h>

/* Language integers are not C ints. Reject out-of-range components before
 * narrowing; all calendar, timezone-offset and overflow rules belong to XRT. */
static bool component(int64_t value, int* result)
{
    if (value < INT_MIN || value > INT_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    *result = (int)value;
    return true;
}

int64_t xl6TimeFromDateTimeV1(int64_t year, int64_t month, int64_t day,
    int64_t hour, int64_t minute, int64_t second, int64_t microsecond, int64_t offset)
{
    xdatetime fields = {0};
    xtime result = 0;
    fields.Year = year;
    if (!component(month, &fields.Month) || !component(day, &fields.Day) ||
        !component(hour, &fields.Hour) || !component(minute, &fields.Minute) ||
        !component(second, &fields.Second) || !component(microsecond, &fields.Microsecond) ||
        !component(offset, &fields.Offset)) return 0;
    (void)xrtTimeMake(&fields, &result);
    return result;
}

int64_t xl6TimeFromDateV1(int64_t year, int64_t month, int64_t day)
{ return xl6TimeFromDateTimeV1(year, month, day, 0, 0, 0, 0, 0); }

int64_t xl6TimeFromTimeV1(int64_t hour, int64_t minute, int64_t second, int64_t microsecond)
{ return xl6TimeFromDateTimeV1(1970, 1, 1, hour, minute, second, microsecond, 0); }

int64_t xl6TimeFromUnixV1(int64_t seconds)
{ xtime result = 0; (void)xrtTimeFromUnix(seconds, &result); return result; }
int64_t xl6TimeFromUnixMsV1(int64_t milliseconds)
{ xtime result = 0; (void)xrtTimeFromUnixMs(milliseconds, &result); return result; }
int64_t xl6TimeUnixV1(int64_t value) { return xrtTimeUnix(value); }
int64_t xl6TimeUnixMsV1(int64_t value) { return xrtTimeUnixMs(value); }
int64_t xl6TimeMicrosecondV1(int64_t value) { return xrtMicrosecond(value); }
int64_t xl6TimeNowV1(void) { return xrtNow(); }
uint64_t xl6TimeMonotonicV1(void) { return xrtClock(); }
void xl6TimeSleepUsV1(uint64_t microseconds) { xrtSleepUs(microseconds); }
void xl6TimeSleepUntilV1(uint64_t deadline) { xrtSleepUntil(deadline); }
bool xl6TimeNearV1(int64_t left, int64_t right, uint64_t tolerance)
{ return xrtTimeNear(left, right, tolerance); }

int64_t xl6TimeAddV1(int64_t value, int64_t amount, int64_t unit)
{
    xtime result = 0;
    if (unit < XTIME_UNIT_MICROSECOND || unit > XTIME_UNIT_YEAR) {
        xrtSetErrorKind(XERR_RANGE);
        return 0;
    }
    (void)xrtTimeAdd(value, amount, (xtimeunit)unit, &result);
    return result;
}
int64_t xl6TimeDiffV1(int64_t start, int64_t end, int64_t unit)
{
    int64 result = 0;
    if (unit < XTIME_UNIT_MICROSECOND || unit > XTIME_UNIT_YEAR) {
        xrtSetErrorKind(XERR_RANGE);
        return 0;
    }
    (void)xrtTimeDiff(start, end, (xtimeunit)unit, &result);
    return result;
}
