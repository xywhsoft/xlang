#include "include/xl_clock.h"
#define XRT_MODULE_TIME_LOCAL
#define XRT_MODULE_TIME_TEXT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出一个与 xlang 示例相同的验证结果。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 验证并释放 XRT 返回的所有权字符串。 */
static int owned_text_eq(char* text, const char* expected)
{
    int ok = text != NULL && strcmp(text, expected) == 0;

    xrtFree(text);
    return ok;
}



/* 返回以星期一开始的月内周序号。 */
static int week_of_month(xtime value)
{
    xdatetime dateTime;
    xtime first = 0;
    int firstWeekday;
    int firstDayOffset;

    if ( !xrtTimeSplit(value, &dateTime) ||
         !xrtDate(dateTime.Year, dateTime.Month, 1, &first) ) {
        return 0;
    }
    firstWeekday = xrtWeekday(first);
    firstDayOffset = firstWeekday == XTIME_SUNDAY ? 6 : firstWeekday - 1;
    return ((dateTime.Day - 1 + firstDayOffset) / 7) + 1;
}



/* 使用当前 XRT API 完成与 xlang 时间标准库示例相同的操作。 */
int main(void)
{
    xtime releaseDay = 0;
    xtime releaseClock = 0;
    xtime releaseMoment = 0;
    xtime defaultMoment = 0;
    bool constructOk;

    constructOk =
        xrtDate(2026, 6, 18, &releaseDay) &&
        xrtDateTime(1970, 1, 1, 9, 30, 0, 0, &releaseClock) &&
        xrtDateTime(2026, 6, 18, 9, 30, 0, 0, &releaseMoment) &&
        xrtDateTime(2026, 6, 18, 0, 0, 0, 0, &defaultMoment);
    require_line("construct",
        constructOk &&
        releaseMoment != 0 &&
        defaultMoment == releaseDay);

    {
        xtime parsedMoment = 0;
        xtime parsedPattern = 0;
        xtime tryMoment = 0;
        xtime tryZero = -1;
        xtime zeroMoment = 0;
        xtime tryPattern = 0;
        xtime ignored = 0;
        bool parsedOk = xrtTimeParseAny(
            XRT_STR_LITERAL("2026-06-18 09:30:00"), &parsedMoment);
        bool parsedPatternOk = xrtTimeParse(
            XRT_STR_LITERAL("2026/06/18 09:30:00"),
            XRT_STR_LITERAL("%Y/%m/%d %H:%M:%S"), &parsedPattern);
        bool tryOk = xrtTimeParseAny(
            XRT_STR_LITERAL("2026-06-18 09:30:00"), &tryMoment);
        bool tryZeroOk = xrtTimeParseAny(
            XRT_STR_LITERAL("00:00:00"), &tryZero);
        bool zeroOk = xrtTimeParseAny(
            XRT_STR_LITERAL("00:00:00"), &zeroMoment);
        bool tryBadOk = xrtTimeParseAny(
            XRT_STR_LITERAL("bad"), &ignored);
        bool tryPatternOk;
        bool tryPatternBadOk;

        xrtClearError();
        tryPatternOk = xrtTimeParse(
            XRT_STR_LITERAL("2026/06/18 09:30:00"),
            XRT_STR_LITERAL("%Y/%m/%d %H:%M:%S"), &tryPattern);
        tryPatternBadOk = xrtTimeParse(
            XRT_STR_LITERAL("bad"), XRT_STR_LITERAL("%Y/%m/%d"), &ignored);
        xrtClearError();
        require_line("parse",
            parsedOk && parsedMoment == releaseMoment &&
            parsedPatternOk && parsedPattern == releaseMoment &&
            tryOk && tryMoment == releaseMoment &&
            tryZeroOk && zeroOk && tryZero == zeroMoment &&
            !tryBadOk &&
            tryPatternOk && tryPattern == releaseMoment &&
            !tryPatternBadOk);
    }

    require_line("parts",
        xrtYear(releaseMoment) == 2026 &&
        xrtMonth(releaseMoment) == 6 &&
        xrtDay(releaseMoment) == 18 &&
        xrtHour(releaseMoment) == 9 &&
        xrtMinute(releaseMoment) == 30 &&
        xrtSecond(releaseMoment) == 0 &&
        xrtWeekday(releaseMoment) >= XTIME_SUNDAY &&
        xrtDayOfYear(releaseMoment) == 169 &&
        xrtQuarter(releaseMoment) == 2);

    {
        xtime nextYear = 0;
        xtime nextMonth = 0;
        xtime nextDay = 0;
        xtime nextHour = 0;
        xtime nextMinute = 0;
        xtime nextSecond = 0;
        xtime nextWeek = 0;
        xtime nextQuarter = 0;
        bool addOk =
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_YEAR, &nextYear) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_MONTH, &nextMonth) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_DAY, &nextDay) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_HOUR, &nextHour) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_MINUTE, &nextMinute) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_SECOND, &nextSecond) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_WEEK, &nextWeek) &&
            xrtTimeAdd(releaseMoment, 1, XTIME_UNIT_QUARTER, &nextQuarter);

        require_line("add",
            addOk &&
            xrtYear(nextYear) == 2027 &&
            xrtMonth(nextMonth) == 7 &&
            xrtDay(nextDay) == 19 &&
            xrtHour(nextHour) == 10 &&
            xrtMinute(nextMinute) == 31 &&
            xrtSecond(nextSecond) == 1 &&
            xrtDay(nextWeek) == 25 &&
            xrtMonth(nextQuarter) == 9);

        {
            int64 years = 0;
            int64 months = 0;
            int64 days = 0;
            int64 hours = 0;
            int64 minutes = 0;
            int64 seconds = 0;
            int64 weeks = 0;
            int64 quarters = 0;
            bool diffOk =
                xrtTimeDiff(releaseMoment, nextYear,
                    XTIME_UNIT_YEAR, &years) &&
                xrtTimeDiff(releaseMoment, nextYear,
                    XTIME_UNIT_MONTH, &months) &&
                xrtTimeDiff(releaseMoment, nextDay,
                    XTIME_UNIT_DAY, &days) &&
                xrtTimeDiff(releaseMoment, nextHour,
                    XTIME_UNIT_HOUR, &hours) &&
                xrtTimeDiff(releaseMoment, nextMinute,
                    XTIME_UNIT_MINUTE, &minutes) &&
                xrtTimeDiff(releaseMoment, nextSecond,
                    XTIME_UNIT_SECOND, &seconds) &&
                xrtTimeDiff(releaseMoment, nextWeek,
                    XTIME_UNIT_WEEK, &weeks) &&
                xrtTimeDiff(releaseMoment, nextYear,
                    XTIME_UNIT_QUARTER, &quarters);

            require_line("diff",
                diffOk && years == 1 && months == 12 &&
                days == 1 && hours == 1 && minutes == 1 &&
                seconds == 1 && weeks == 1 && quarters == 4);
        }

        require_line("compare",
            xrtDatePart(releaseMoment) == releaseDay &&
            xrtTimePart(releaseMoment) == releaseClock &&
            xrtTimeSameDay(releaseMoment, nextHour) &&
            xrtTimeSameMonth(releaseMoment, nextDay) &&
            xrtTimeSameYear(releaseMoment, nextMonth) &&
            xrtTimeIn(releaseMoment, releaseDay, nextWeek) &&
            xrtTimeOverlap(releaseDay, nextWeek, releaseMoment, nextWeek) &&
            xrtTimeNear(releaseMoment, releaseMoment,
                (uint64)(10 * XRT_TIME_SECOND)));

        {
            xtime monthStart = 0;
            xtime monthEnd = 0;
            xtime yearStart = 0;
            xtime yearEnd = 0;
            xtime weekStart = 0;
            xtime weekEnd = 0;
            int64 weekYear = 0;
            int week = 0;
            int weekday = 0;
            bool calendarOk =
                xrtMonthRange(releaseMoment, &monthStart, &monthEnd) &&
                xrtYearRange(releaseMoment, &yearStart, &yearEnd) &&
                xrtWeekRange(releaseMoment, XTIME_MONDAY,
                    &weekStart, &weekEnd) &&
                xrtISOWeek(releaseMoment, &weekYear, &week, &weekday);

            require_line("calendar",
                calendarOk &&
                xrtIsLeapYear(2024) &&
                !xrtIsLeapYear(2023) &&
                xrtDaysInMonth(2024, 2) == 29 &&
                xrtDaysInYear(2024) == 366 &&
                xrtMonth(monthStart) == 6 &&
                xrtDay(monthStart) == 1 &&
                xrtDay(monthEnd - XRT_TIME_DAY) == 30 &&
                xrtMonth(yearStart) == 1 &&
                xrtMonth(yearEnd - XRT_TIME_DAY) == 12 &&
                weekStart <= releaseMoment &&
                (weekEnd - XRT_TIME_DAY) >= releaseMoment &&
                weekYear == 2026 && week > 0 && weekday > 0 &&
                week_of_month(releaseMoment) > 0);
        }
    }

    {
        int64 unixSeconds = xrtTimeUnix(releaseMoment);
        xtime unixMoment = 0;
        xtime utcMoment = 0;
        xtime now = xrtNow();
        xdatetime localMoment;
        uint64 clock = xl6MonotonicMillisecondsV1();
        xdatetime localDateTime;
        bool systemOk =
            xrtTimeFromUnix(unixSeconds, &unixMoment) &&
            xrtTimeLocal(releaseMoment, &localMoment) &&
            xrtTimeFromLocal(&localMoment, XTIME_FOLD_REJECT, &utcMoment) &&
            xrtTimeLocal(now, &localDateTime);

        require_line("system",
            systemOk &&
            unixMoment == releaseMoment &&
            utcMoment == releaseMoment &&
            xrtYear(xrtDatePart(now)) >= 1970 &&
            xl6MonotonicMillisecondsV1() >= clock &&
            xrtYear(now) >= 1970 &&
            localDateTime.Offset > -86400 &&
            localDateTime.Offset < 86400);
    }

    require_line("format",
        owned_text_eq(xrtTimeFormat(releaseMoment, 0,
            XRT_STR_LITERAL("%Y-%m-%d %H:%M:%S")),
            "2026-06-18 09:30:00") &&
        owned_text_eq(xrtTimeFormat(releaseMoment, 0,
            XRT_STR_LITERAL("%Y/%m/%d %H:%M:%S")),
            "2026/06/18 09:30:00") &&
        owned_text_eq(xrtTimeFormat(releaseMoment, 0,
            XRT_STR_LITERAL("%Y-%m-%d")), "2026-06-18") &&
        owned_text_eq(xrtTimeFormat(releaseMoment, 0,
            XRT_STR_LITERAL("%H:%M:%S")), "09:30:00") &&
        owned_text_eq(xrtTimeRFC3339(releaseMoment, 0),
            "2026-06-18T09:30:00Z"));

    {
        xtime precise = INT64_C(1709251137123456), parsed = 0, httpParsed = 0, fieldsTime = 0;
        char protocol[64], http[64], fieldText[64];
        size_t protocolSize = xrtTimeWriteRFC3339(protocol, sizeof(protocol), precise, 28800);
        size_t httpSize = xrtTimeWriteHTTPDate(http, sizeof(http), releaseMoment);
        xdatetime fields;
        bool protocolOk = protocolSize != XRT_NPOS && httpSize != XRT_NPOS &&
            strcmp(protocol, "2024-03-01T07:58:57.123456+08:00") == 0 &&
            xrtTimeParseRFC3339((xstrview){protocol, protocolSize}, &parsed) && parsed == precise &&
            xrtTimeParseHTTPDate((xstrview){http, httpSize}, &httpParsed) && httpParsed == releaseMoment &&
            xrtTimeTryParseHTTPDate((xstrview){http, httpSize}, &httpParsed) &&
            xrtDateTimeParse(XRT_STR_LITERAL("2024-03-01 07:58:57.123456 +08:00"),
                XRT_STR_LITERAL("%F %T.%f %:z"), &fields) &&
            fields.Offset == 28800 && xrtTimeMake(&fields, &fieldsTime) && fieldsTime == precise &&
            xrtDateTimeWrite(fieldText, sizeof(fieldText), &fields, XRT_STR_LITERAL("%F %T.%f %:z")) != XRT_NPOS &&
            strcmp(fieldText, "2024-03-01 07:58:57.123456 +08:00") == 0 &&
            xrtTimeParseAny((xstrview){"2024-02-29\0ignored", 10}, &parsed) &&
            xrtYear(parsed) == 2024 && xrtMonth(parsed) == 2 && xrtDay(parsed) == 29;
        require_line("protocol", protocolOk);
    }

    require_line("timer", xrtTimer() >= 0.0);
    xrtSleepUs(0);
    return 0;
}
