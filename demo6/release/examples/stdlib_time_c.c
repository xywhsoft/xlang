#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int owned_text_eq(char* text, const char* expected)
{
    int ok = text != NULL && strcmp(text, expected) == 0;
    xrtFree(text);
    return ok;
}

static int owned_text_not_empty(char* text)
{
    int ok = text != NULL && text[0] != '\0';
    xrtFree(text);
    return ok;
}

int main(void)
{
    xtime releaseDay = xrtDateSerial(2026, 6, 18);
    xtime releaseClock = xrtTimeSerial(9, 30, 0);
    xtime releaseMoment = xrtDateTimeSerial(2026, 6, 18, 9, 30, 0);
    xtime defaultMoment = xrtDateTimeSerial(2026, 6, 18, 0, 0, 0);

    require_line("construct",
        releaseDay == xrtStrToTime("2026-06-18", 0) &&
        releaseClock == xrtStrToTime("09:30:00", 0) &&
        releaseMoment == xrtStrToTime("2026-06-18 09:30:00", 0) &&
        defaultMoment == releaseDay);

    xtime parsedMoment = xrtStrToTime("2026-06-18 09:30:00", 0);
    xtime parsedPattern = xrtTimeParse("2026/06/18 09:30:00", "yyyy/mm/dd hh:mm:ss");
    xtime tryMoment = 0;
    xtime tryZero = -1;
    xtime tryPattern = 0;
    xtime ignored = 0;
    int tryOk = xrtTryStrToTime("2026-06-18 09:30:00", 0, &tryMoment);
    int tryZeroOk = xrtTryStrToTime("00:00:00", 0, &tryZero);
    int tryBadOk = xrtTryStrToTime("bad", 0, &ignored);
    int tryPatternOk = xrtTryTimeParse("2026/06/18 09:30:00", "yyyy/mm/dd hh:mm:ss", &tryPattern);
    int tryPatternBadOk = xrtTryTimeParse("bad", "yyyy/mm/dd", &ignored);

    require_line("parse",
        parsedMoment == releaseMoment &&
        parsedPattern == releaseMoment &&
        tryOk && tryMoment == releaseMoment &&
        tryZeroOk && tryZero == xrtStrToTime("00:00:00", 0) &&
        !tryBadOk &&
        tryPatternOk && tryPattern == releaseMoment &&
        !tryPatternBadOk);

    require_line("parts",
        xrtYear(releaseMoment) == 2026 &&
        xrtMonth(releaseMoment) == 6 &&
        xrtDay(releaseMoment) == 18 &&
        xrtHour(releaseMoment) == 9 &&
        xrtMinute(releaseMoment) == 30 &&
        xrtSecond(releaseMoment) == 0 &&
        xrtWeekday(releaseMoment) >= 0 &&
        xrtDayOfYear(releaseMoment) == 169 &&
        xrtQuarter(releaseMoment) == 2);

    xtime nextYear = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, 1, releaseMoment);
    xtime nextMonth = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, releaseMoment);
    xtime nextDay = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, releaseMoment);
    xtime nextHour = xrtDateAdd(XRT_TIME_INTERVAL_HOUR, 1, releaseMoment);
    xtime nextMinute = xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, 1, releaseMoment);
    xtime nextSecond = xrtDateAdd(XRT_TIME_INTERVAL_SECOND, 1, releaseMoment);
    xtime nextWeek = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 1, releaseMoment);
    xtime nextQuarter = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 1, releaseMoment);

    require_line("add",
        xrtYear(nextYear) == 2027 &&
        xrtMonth(nextMonth) == 7 &&
        xrtDay(nextDay) == 19 &&
        xrtHour(nextHour) == 10 &&
        xrtMinute(nextMinute) == 31 &&
        xrtSecond(nextSecond) == 1 &&
        xrtDay(nextWeek) == 25 &&
        xrtMonth(nextQuarter) == 9);

    require_line("diff",
        xrtDateDiff(XRT_TIME_INTERVAL_YEAR, releaseMoment, nextYear) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_MONTH, releaseMoment, nextYear) == 12 &&
        xrtDateDiff(XRT_TIME_INTERVAL_DAY, releaseMoment, nextDay) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_HOUR, releaseMoment, nextHour) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_MINUTE, releaseMoment, nextMinute) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_SECOND, releaseMoment, nextSecond) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_WEEKDAY, releaseMoment, nextWeek) == 1 &&
        xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, releaseMoment, nextYear) == 4);

    require_line("compare",
        xrtDatePart(releaseMoment) == releaseDay &&
        xrtTimePart(releaseMoment) == releaseClock &&
        xrtIsSameDay(releaseMoment, nextHour) &&
        xrtIsSameMonth(releaseMoment, nextDay) &&
        xrtIsSameYear(releaseMoment, nextMonth) &&
        xrtTimeInRange(releaseMoment, releaseDay, nextWeek) &&
        xrtTimeRangeOverlap(releaseDay, nextWeek, releaseMoment, nextWeek) &&
        xrtTimeApprox(releaseMoment, releaseMoment));

    require_line("calendar",
        xrtIsLeapYear(2024) &&
        !xrtIsLeapYear(2023) &&
        xrtDaysInMonth(2024, 2) == 29 &&
        xrtDaysInYear(2024) == 366 &&
        xrtMonth(xrtFirstDayOfMonth(releaseMoment)) == 6 &&
        xrtDay(xrtFirstDayOfMonth(releaseMoment)) == 1 &&
        xrtDay(xrtLastDayOfMonth(releaseMoment)) == 30 &&
        xrtMonth(xrtFirstDayOfYear(releaseMoment)) == 1 &&
        xrtMonth(xrtLastDayOfYear(releaseMoment)) == 12 &&
        xrtFirstDayOfWeek(releaseMoment, 1) <= releaseMoment &&
        xrtLastDayOfWeek(releaseMoment, 1) >= releaseMoment &&
        xrtWeekOfYear(releaseMoment) > 0 &&
        xrtWeekOfMonth(releaseMoment) > 0);

    int64 unixSeconds = xrtToUnixTime(releaseMoment);
    xtime unixMoment = xrtFromUnixTime(unixSeconds);
    xtime utcMoment = xrtLocalToUTC(releaseMoment);
    xtime localMoment = xrtUTCToLocal(utcMoment);

    require_line("system",
        unixMoment == releaseMoment &&
        localMoment == releaseMoment &&
        xrtYear(xrtNow()) >= 1970 &&
        xrtYear(xrtDate()) >= 1970 &&
        xrtHour(xrtTime()) >= 0 &&
        xrtYear(xrtNowUTC()) >= 1970 &&
        xrtTimezoneOffset() == xrtTimezoneOffset());

    require_line("format",
        owned_text_eq(xrtTimeToStr(releaseMoment, XRT_TIME_FORMAT_DATETIME), "2026-06-18 09:30:00") &&
        owned_text_eq(xrtTimeFormat(releaseMoment, "yyyy/mm/dd hh:mm:ss"), "2026/06/18 09:30:00") &&
        owned_text_eq(xrtTimeToStr(releaseMoment, XRT_TIME_FORMAT_DATE), "2026-06-18") &&
        owned_text_eq(xrtTimeToStr(releaseMoment, XRT_TIME_FORMAT_TIME), "09:30:00") &&
        owned_text_not_empty(xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_SECOND, 90, releaseMoment), releaseMoment)));

    require_line("timer", xrtTimer() >= 0.0);
    xrtSleep(0);

    return 0;
}
