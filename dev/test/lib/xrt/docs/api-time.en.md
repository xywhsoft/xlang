# Time Processing Library

> Date and time calculation and formatting functions

[English](api-time.en.md) | [中文](api-time.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [Time Retrieval](#time-retrieval)
- [Time Construction](#time-construction)
- [Time Extraction](#time-extraction)
- [Time Decoding](#time-decoding)
- [Time Calculation](#time-calculation)
- [Time Formatting](#time-formatting)
- [Date Utilities](#date-utilities)
- [Use Cases](#use-cases)

---

## Constant Definitions

### Time Unit Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XRT_TIME_MINUTE` | `60` | Seconds in 1 minute |
| `XRT_TIME_HOUR` | `3600` | Seconds in 1 hour |
| `XRT_TIME_DAY` | `86400` | Seconds in 1 day |
| `XRT_TIME_YEAR` | `31536000` | Seconds in 1 common year |
| `XRT_TIME_LEAPYEAR` | `31622400` | Seconds in 1 leap year |
| `XRT_TIME_400YEAR` | `12622780800` | Seconds in 400 years (97 leap years + 303 common years) |
| `XRT_TIME_19700101` | `62167219200` | Timestamp of January 1, 1970 |

### Time Interval Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XRT_TIME_INTERVAL_YEAR` | `1` | Year |
| `XRT_TIME_INTERVAL_MONTH` | `2` | Month |
| `XRT_TIME_INTERVAL_DAY` | `3` | Day |
| `XRT_TIME_INTERVAL_HOUR` | `4` | Hour |
| `XRT_TIME_INTERVAL_MINUTE` | `5` | Minute |
| `XRT_TIME_INTERVAL_SECOND` | `6` | Second |
| `XRT_TIME_INTERVAL_WEEKDAY` | `7` | Week |
| `XRT_TIME_INTERVAL_QUARTER` | `8` | Quarter |

### Time Format Constants

| Constant | Value | Output Format |
|----------|-------|---------------|
| `XRT_TIME_FORMAT_DATETIME` | `0` | `YYYY-MM-DD HH:MM:SS` |
| `XRT_TIME_FORMAT_DATE` | `1` | `YYYY-MM-DD` |
| `XRT_TIME_FORMAT_TIME` | `2` | `HH:MM:SS` |

---

## Time Retrieval

### xrtNow

Get current date and time.

**Prototype:**
```c
XXAPI xtime xrtNow();
```

**Return Value:**
- Timestamp of current date and time (seconds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    printf("Timestamp: %" PRId64 "\n", now);
    
    // Convert to readable string
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDate

Get current date (without time part).

**Prototype:**
```c
XXAPI xtime xrtDate();
```

**Return Value:**
- Timestamp of current date (hour/minute/second are 00:00:00)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime today = xrtDate();
    str dateStr = xrtTimeToStr(today, XRT_TIME_FORMAT_DATE);
    printf("Today: %s\n", dateStr);
    xrtFree(dateStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTime

Get current time (without date part).

**Prototype:**
```c
XXAPI xtime xrtTime();
```

**Return Value:**
- Seconds elapsed today (0-86399)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime currentTime = xrtTime();
    printf("Seconds elapsed today: %" PRId64 "\n", currentTime);
    
    str timeStr = xrtTimeToStr(currentTime, XRT_TIME_FORMAT_TIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimer

Get high-precision timer (for performance measurement).

**Prototype:**
```c
XXAPI double xrtTimer();
```

**Return Value:**
- High-precision time (seconds, floating point)

**Description:**
- Windows: Uses `QueryPerformanceCounter`
- Linux/macOS: Uses `clock_gettime(CLOCK_MONOTONIC)`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void HeavyWork() {
    // Simulate time-consuming operation
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    HeavyWork();
    double end = xrtTimer();
    
    printf("Elapsed: %.6f seconds\n", end - start);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Returns monotonically increasing time, suitable for calculating time intervals
- Not affected by system time adjustments

---

### xrtSleep

Millisecond-level delay.

**Prototype:**
```c
XXAPI void xrtSleep(uint32 ms);
```

**Parameters:**
- `ms` - Delay in milliseconds

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("Starting wait...\n");
    xrtSleep(1000);  // Wait 1 second
    printf("After 1 second\n");
    
    xrtSleep(500);   // Wait 500 milliseconds
    printf("Another 0.5 seconds passed\n");
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `Sleep()`
- Linux/macOS uses `usleep()`

---

### xrtNowStr / xrtDateStr / xrtTimeStr

Get string formatted current time.

**Prototypes:**
```c
XXAPI str xrtNowStr();   // Returns "YYYY-MM-DD HH:MM:SS"
XXAPI str xrtDateStr();  // Returns "YYYY-MM-DD"
XXAPI str xrtTimeStr();  // Returns "HH:MM:SS"
```

**Return Value:**
- Formatted time string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str now = xrtNowStr();
    str date = xrtDateStr();
    str time = xrtTimeStr();
    
    printf("Current datetime: %s\n", now);   // "2025-12-03 15:30:45"
    printf("Current date: %s\n", date);      // "2025-12-03"
    printf("Current time: %s\n", time);      // "15:30:45"
    
    xrtFree(now);
    xrtFree(date);
    xrtFree(time);
    
    xrtUnit();
    return 0;
}
```

---

## Time Construction

### xrtDateSerial

Construct date timestamp.

**Prototype:**
```c
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
```

**Parameters:**
- `iYear` - Year (negative values for BC)
- `iMonth` - Month (1-12)
- `iDay` - Day (1-31)

**Return Value:**
- Timestamp (seconds)
- Returns 0 and sets error message for invalid month

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct date
    xtime date = xrtDateSerial(2025, 1, 10);
    str dateStr = xrtTimeToStr(date, XRT_TIME_FORMAT_DATE);
    printf("Date: %s\n", dateStr);  // "2025-01-10"
    xrtFree(dateStr);
    
    // BC date (negative year)
    xtime bc = xrtDateSerial(-100, 3, 15);
    printf("BC timestamp: %" PRId64 "\n", bc);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeSerial

Construct time timestamp (seconds of day).

**Prototype:**
```c
XXAPI xtime xrtTimeSerial(int iHour, int iMin, int iSec);
```

**Parameters:**
- `iHour` - Hour (0-23)
- `iMin` - Minute (0-59)
- `iSec` - Second (0-59)

**Return Value:**
- Timestamp (seconds of day, 0-86399)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct time
    xtime time = xrtTimeSerial(15, 30, 45);
    printf("Seconds of day: %" PRId64 "\n", time);  // 55845 = 15*3600 + 30*60 + 45
    
    str timeStr = xrtTimeToStr(time, XRT_TIME_FORMAT_TIME);
    printf("Time: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateTimeSerial

Construct complete date+time timestamp.

**Prototype:**
```c
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
```

**Parameters:**
- `iYear` - Year
- `iMonth` - Month (1-12)
- `iDay` - Day (1-31)
- `iHour` - Hour (0-23)
- `iMinute` - Minute (0-59)
- `iSecond` - Second (0-59)

**Return Value:**
- Complete timestamp

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct complete datetime
    xtime dt = xrtDateTimeSerial(2025, 12, 25, 10, 30, 0);
    str dtStr = xrtTimeToStr(dt, XRT_TIME_FORMAT_DATETIME);
    printf("Christmas: %s\n", dtStr);  // "2025-12-25 10:30:00"
    xrtFree(dtStr);
    
    // Result equals xrtDateSerial + xrtTimeSerial
    xtime d = xrtDateSerial(2025, 12, 25);
    xtime t = xrtTimeSerial(10, 30, 0);
    printf("Equal: %s\n", (d + t == dt) ? "Yes" : "No");  // "Yes"
    
    xrtUnit();
    return 0;
}
```

---

## Time Calculation

### xrtDateAdd

Add time units.

**Prototype:**
```c
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
```

**Parameters:**
- `interval` - Interval type (use `XRT_TIME_INTERVAL_*` constants)
  - `XRT_TIME_INTERVAL_YEAR` (1) - Year
  - `XRT_TIME_INTERVAL_MONTH` (2) - Month
  - `XRT_TIME_INTERVAL_DAY` (3) - Day
  - `XRT_TIME_INTERVAL_HOUR` (4) - Hour
  - `XRT_TIME_INTERVAL_MINUTE` (5) - Minute
  - `XRT_TIME_INTERVAL_SECOND` (6) - Second
  - `XRT_TIME_INTERVAL_WEEKDAY` (7) - Week (7 days)
  - `XRT_TIME_INTERVAL_QUARTER` (8) - Quarter (3 months)
- `iValue` - Increment (can be negative)
- `iTime` - Base time

**Return Value:**
- Calculated timestamp

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    str nowStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current: %s\n", nowStr);
    xrtFree(nowStr);
    
    // Add 1 day
    xtime tomorrow = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, now);
    str s1 = xrtTimeToStr(tomorrow, XRT_TIME_FORMAT_DATE);
    printf("Tomorrow: %s\n", s1);
    xrtFree(s1);
    
    // Add 1 week
    xtime next_week = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 1, now);
    str s2 = xrtTimeToStr(next_week, XRT_TIME_FORMAT_DATE);
    printf("Next week: %s\n", s2);
    xrtFree(s2);
    
    // Subtract 1 month
    xtime last_month = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -1, now);
    str s3 = xrtTimeToStr(last_month, XRT_TIME_FORMAT_DATE);
    printf("Last month: %s\n", s3);
    xrtFree(s3);
    
    // Add 1 quarter
    xtime next_quarter = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 1, now);
    str s4 = xrtTimeToStr(next_quarter, XRT_TIME_FORMAT_DATE);
    printf("Next quarter: %s\n", s4);
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateDiff

Calculate time difference.

**Prototype:**
```c
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

**Parameters:**
- `interval` - Interval unit (same as `xrtDateAdd`, but **does NOT support** `XRT_TIME_INTERVAL_WEEKDAY`)
- `iTime1` - Time 1
- `iTime2` - Time 2

**Return Value:**
- Time difference (iTime2 - iTime1)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 12, 31);
    
    // Days difference
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, start, end);
    printf("Days difference: %" PRId64 "\n", days);  // 364
    
    // Months difference
    int64 months = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, start, end);
    printf("Months difference: %" PRId64 "\n", months);  // 11
    
    // Quarters difference
    int64 quarters = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, start, end);
    printf("Quarters difference: %" PRId64 "\n", quarters);  // 3
    
    // Hours difference
    xtime t1 = xrtDateTimeSerial(2025, 1, 1, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 1, 1, 17, 30, 0);
    int64 hours = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, t1, t2);
    printf("Hours difference: %" PRId64 "\n", hours);  // 9
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `XRT_TIME_INTERVAL_WEEKDAY` is not supported, returns 0
- Result is iTime2 - iTime1, can be negative

---

## Time Formatting

### xrtTimeToStr

Convert time to string

**Prototype:**
```c
XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
```

**Parameters:**
- `iTime` - Timestamp
- `iFormat` - Format type
  - `0` - `YYYY-MM-DD HH:MM:SS`
  - `1` - `YYYY-MM-DD`
  - `2` - `HH:MM:SS`
  - `3` - `YYYYMMDDHHMMSS`

**Return Value:**
- Formatted time string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
xtime now = xrtNow();
str s0 = xrtTimeToStr(now, 0);  // "2025-01-10 15:30:45"
str s1 = xrtTimeToStr(now, 1);  // "2025-01-10"
str s2 = xrtTimeToStr(now, 2);  // "15:30:45"
str s3 = xrtTimeToStr(now, 3);  // "20250110153045"
xrtFree(s0); xrtFree(s1); xrtFree(s2); xrtFree(s3);
```

---

## Time Extraction

### xrtYear

Get year from time.

**Prototype:**
```c
XXAPI int64 xrtYear(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Year (negative for BC)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int64 year = xrtYear(now);
    printf("Current year: %" PRId64 "\n", year);  // 2025
    
    xrtUnit();
    return 0;
}
```

---

### xrtMonth

Get month from time.

**Prototype:**
```c
XXAPI int xrtMonth(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Month (1-12)

---

### xrtDay

Get day from time.

**Prototype:**
```c
XXAPI int xrtDay(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Day (1-31)

---

### xrtHour

Get hour from time.

**Prototype:**
```c
XXAPI int xrtHour(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Hour (0-23)

---

### xrtMinute

Get minute from time.

**Prototype:**
```c
XXAPI int xrtMinute(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Minute (0-59)

---

### xrtSecond

Get second from time.

**Prototype:**
```c
XXAPI int xrtSecond(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Second (0-59)

---

### xrtWeekday

Get weekday from time.

**Prototype:**
```c
XXAPI int xrtWeekday(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Weekday (0=Sunday, 1=Monday, ..., 6=Saturday)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int weekday = xrtWeekday(now);
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    printf("Today is %s\n", days[weekday]);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDayOfYear

Get day of year from time.

**Prototype:**
```c
XXAPI int xrtDayOfYear(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Day of year (1-366)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int day_of_year = xrtDayOfYear(now);
    printf("Today is day %d of the year\n", day_of_year);
    
    // January 1st is day 1
    xtime jan1 = xrtDateSerial(2025, 1, 1);
    printf("January 1st: Day %d\n", xrtDayOfYear(jan1));  // 1
    
    xrtUnit();
    return 0;
}
```

**Complete Time Extraction Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    printf("Current time: %" PRId64 "-", xrtYear(now));
    printf("%02d-%02d ", xrtMonth(now), xrtDay(now));
    printf("%02d:%02d:%02d\n", xrtHour(now), xrtMinute(now), xrtSecond(now));
    printf("Weekday: %d\n", xrtWeekday(now));
    printf("Day of year: %d\n", xrtDayOfYear(now));
    
    xrtUnit();
    return 0;
}
```

---

## Time Decoding

### xrtDecodeSerial

Decode all time components at once.

**Prototype:**
```c
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, 
                           int* pHour, int* pMinute, int* pSecond, 
                           int* pWeekday, int* pDayOfYear);
```

**Parameters:**
- `iTime` - Timestamp
- `pYear` - Year output pointer (can be NULL)
- `pMonth` - Month output pointer (can be NULL)
- `pDay` - Day output pointer (can be NULL)
- `pHour` - Hour output pointer (can be NULL)
- `pMinute` - Minute output pointer (can be NULL)
- `pSecond` - Second output pointer (can be NULL)
- `pWeekday` - Weekday output pointer (can be NULL)
- `pDayOfYear` - Day of year output pointer (can be NULL)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // Decode all fields
    int64 year;
    int month, day, hour, minute, second, weekday, dayOfYear;
    xrtDecodeSerial(now, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);
    
    printf("Full decode:\n");
    printf("Date: %" PRId64 "-%02d-%02d\n", year, month, day);
    printf("Time: %02d:%02d:%02d\n", hour, minute, second);
    printf("Weekday: %d\n", weekday);
    printf("Day of year: %d\n", dayOfYear);
    
    // Decode only some fields
    int64 y;
    int m, d;
    xrtDecodeSerial(now, &y, &m, &d, NULL, NULL, NULL, NULL, NULL);
    printf("Date only: %" PRId64 "-%02d-%02d\n", y, m, d);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- More efficient than multiple calls to individual extraction functions
- Fields not needed can pass `NULL`

---

## Date Utilities

### xrtIsLeapYear

Check if year is a leap year.

**Prototype:**
```c
XXAPI bool xrtIsLeapYear(int iYear);
```

**Parameters:**
- `iYear` - Year

**Return Value:**
- `TRUE` - Leap year
- `FALSE` - Common year

**Rules:**
- Divisible by 400 → Leap year
- Divisible by 100 → Common year
- Divisible by 4 → Leap year
- Others → Common year

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %s\n", xrtIsLeapYear(2024) ? "Leap year" : "Common year");  // Leap year
    printf("2025: %s\n", xrtIsLeapYear(2025) ? "Leap year" : "Common year");  // Common year
    printf("2000: %s\n", xrtIsLeapYear(2000) ? "Leap year" : "Common year");  // Leap year
    printf("1900: %s\n", xrtIsLeapYear(1900) ? "Leap year" : "Common year");  // Common year
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInMonth

Get number of days in a month.

**Prototype:**
```c
XXAPI int xrtDaysInMonth(int iYear, int iMonth);
```

**Parameters:**
- `iYear` - Year
- `iMonth` - Month (1-12)

**Return Value:**
- Number of days (28-31)
- Returns 0 for invalid month

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // February days depend on leap year
    printf("Feb 2024: %d days\n", xrtDaysInMonth(2024, 2));  // 29
    printf("Feb 2025: %d days\n", xrtDaysInMonth(2025, 2));  // 28
    
    // Other months
    printf("January: %d days\n", xrtDaysInMonth(2025, 1));   // 31
    printf("April: %d days\n", xrtDaysInMonth(2025, 4));     // 30
    printf("December: %d days\n", xrtDaysInMonth(2025, 12)); // 31
    
    // Invalid month
    printf("Invalid month: %d\n", xrtDaysInMonth(2025, 13));  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInYear

Get number of days in a year.

**Prototype:**
```c
XXAPI int xrtDaysInYear(int iYear);
```

**Parameters:**
- `iYear` - Year

**Return Value:**
- Number of days (365 or 366)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %d days\n", xrtDaysInYear(2024));  // 366
    printf("2025: %d days\n", xrtDaysInYear(2025));  // 365
    
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Performance Timer

```c
#include "xrt.h"
#include <stdio.h>

void ProcessData() {
    // Simulate time-consuming operation
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    ProcessData();
    double elapsed = xrtTimer() - start;
    
    printf("Processing time: %.6f seconds\n", elapsed);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Log Timestamp

```c
#include "xrt.h"
#include <stdio.h>

void Log(str message) {
    str timestamp = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
    printf("[%s] %s\n", timestamp, message);
    xrtFree(timestamp);
}

int main() {
    xrtInit();
    
    Log((str)"Application started");
    xrtSleep(1000);
    Log((str)"Processing...");
    xrtSleep(500);
    Log((str)"Application ended");
    
    xrtUnit();
    return 0;
}
```

---

### 3. Expiration Check

```c
#include "xrt.h"
#include <stdio.h>

bool IsExpired(xtime expire_time) {
    return xrtNow() > expire_time;
}

int main() {
    xrtInit();
    
    // Set expiration date to December 31, 2025
    xtime expire = xrtDateSerial(2025, 12, 31);
    
    if (IsExpired(expire)) {
        printf("License has expired\n");
    } else {
        int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, xrtNow(), expire);
        printf("License remaining: %" PRId64 " days\n", days);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. Birthday Calculation

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Assume birthday is May 15, 1990
    xtime birthday = xrtDateSerial(1990, 5, 15);
    xtime today = xrtDate();
    
    // Calculate age
    int64 years = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, birthday, today);
    printf("Age: %" PRId64 " years\n", years);
    
    // Calculate total days
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, birthday, today);
    printf("Days lived: %" PRId64 " days\n", days);
    
    // Next birthday
    int64 this_year = xrtYear(today);
    xtime next_bday = xrtDateSerial(this_year, 5, 15);
    if (next_bday <= today) {
        next_bday = xrtDateSerial(this_year + 1, 5, 15);
    }
    int64 days_until = xrtDateDiff(XRT_TIME_INTERVAL_DAY, today, next_bday);
    printf("Days until next birthday: %" PRId64 " days\n", days_until);
    
    xrtUnit();
    return 0;
}
```

---

### 5. Workday Calculation

```c
#include "xrt.h"
#include <stdio.h>

// Count workdays (excluding weekends)
int CountWorkdays(xtime start, xtime end) {
    int workdays = 0;
    xtime current = start;
    
    while (current <= end) {
        int weekday = xrtWeekday(current);
        if (weekday != 0 && weekday != 6) {  // Not weekend
            workdays++;
        }
        current = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, current);
    }
    
    return workdays;
}

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 1, 31);
    
    int workdays = CountWorkdays(start, end);
    printf("January 2025 workdays: %d days\n", workdays);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documents

- [String Processing](api-string.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#time-processing-library)

</div>
