#include "src/runtime/xl6_time_internal.h"

int64_t xl6TimeFromDateTimeV1(int64_t year, int64_t month, int64_t day,
    int64_t hour, int64_t minute, int64_t second, int64_t microsecond, int64_t offset)
{
    xdatetime fields = {0};
    xtime result = 0;
    if (!xl6TimeFields(&fields, year, month, day, hour, minute, second, microsecond, offset)) return 0;
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

#define FIELD(Name) int64_t xl6Time##Name##V1(int64_t value) { return xrt##Name(value); }
FIELD(Year)
FIELD(Month)
FIELD(Day)
FIELD(Hour)
FIELD(Minute)
FIELD(Second)
FIELD(Weekday)
FIELD(DayOfYear)
FIELD(Quarter)
#undef FIELD
bool xl6TimeLeapV1(int64_t year) { return xrtIsLeapYear(year); }
int64_t xl6TimeDaysInYearV1(int64_t year) { return xrtDaysInYear(year); }
int64_t xl6TimeDaysInMonthV1(int64_t year, int64_t month)
{
    if (month < 1 || month > 12) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return xrtDaysInMonth(year, (int)month);
}
int64_t xl6TimeDatePartV1(int64_t value) { return xrtDatePart(value); }
int64_t xl6TimeTimePartV1(int64_t value) { return xrtTimePart(value); }
bool xl6TimeSameDayV1(int64_t left, int64_t right) { return xrtTimeSameDay(left, right); }
bool xl6TimeSameMonthV1(int64_t left, int64_t right) { return xrtTimeSameMonth(left, right); }
bool xl6TimeSameYearV1(int64_t left, int64_t right) { return xrtTimeSameYear(left, right); }
bool xl6TimeInV1(int64_t value, int64_t start, int64_t end) { return xrtTimeIn(value, start, end); }
bool xl6TimeOverlapV1(int64_t start1, int64_t end1, int64_t start2, int64_t end2)
{ return xrtTimeOverlap(start1, end1, start2, end2); }
double xl6TimeTimerV1(void) { return xrtTimer(); }
void xl6TimeSleepV1(int64_t milliseconds)
{
    if (milliseconds < 0 || (uint64_t)milliseconds > UINT64_MAX / UINT64_C(1000)) {
        xrtSetErrorKind(XERR_RANGE);
        return;
    }
    xrtSleepUs((uint64_t)milliseconds * UINT64_C(1000));
}
XL6_ProductTimeX2V1 xl6TimeMonthRangeV1(int64_t value)
{
    XL6_ProductTimeX2V1 result = {0};
    (void)xrtMonthRange(value, &result.e0, &result.e1);
    return result;
}
XL6_ProductTimeX2V1 xl6TimeYearRangeV1(int64_t value)
{
    XL6_ProductTimeX2V1 result = {0};
    (void)xrtYearRange(value, &result.e0, &result.e1);
    return result;
}
XL6_ProductTimeX2V1 xl6TimeWeekRangeV1(int64_t value, int64_t firstWeekday)
{
    XL6_ProductTimeX2V1 result = {0};
    if (firstWeekday < XTIME_SUNDAY || firstWeekday > XTIME_SATURDAY) {
        xrtSetErrorKind(XERR_RANGE);
        return result;
    }
    (void)xrtWeekRange(value, (int)firstWeekday, &result.e0, &result.e1);
    return result;
}
XL6_ProductI64x3V1 xl6TimeISOWeekV1(int64_t value)
{
    XL6_ProductI64x3V1 result = {0};
    int week = 0, weekday = 0;
    if (xrtISOWeek(value, &result.e0, &week, &weekday)) {
        result.e1 = week;
        result.e2 = weekday;
    }
    return result;
}
