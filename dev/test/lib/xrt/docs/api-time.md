# Time 时间处理库

> 日期时间计算和格式化功能

[English](api-time.en.md) | [中文](api-time.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [时间获取](#时间获取)
- [时间构建](#时间构建)
- [时间提取](#时间提取)
- [时间解码](#时间解码)
- [时间计算](#时间计算)
- [时间格式化](#时间格式化)
- [日期工具](#日期工具)
- [使用场景](#使用场景)

---

## 常量定义

### 时间单位常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_TIME_MINUTE` | `60` | 1分钟的秒数 |
| `XRT_TIME_HOUR` | `3600` | 1小时的秒数 |
| `XRT_TIME_DAY` | `86400` | 1天的秒数 |
| `XRT_TIME_YEAR` | `31536000` | 1平年的秒数 |
| `XRT_TIME_LEAPYEAR` | `31622400` | 1闰年的秒数 |
| `XRT_TIME_400YEAR` | `12622780800` | 400年的秒数（97个闰年+303个平年） |
| `XRT_TIME_19700101` | `62167219200` | 1970年1月1日的时间戳 |

### 时间间隔常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_TIME_INTERVAL_YEAR` | `1` | 年 |
| `XRT_TIME_INTERVAL_MONTH` | `2` | 月 |
| `XRT_TIME_INTERVAL_DAY` | `3` | 日 |
| `XRT_TIME_INTERVAL_HOUR` | `4` | 小时 |
| `XRT_TIME_INTERVAL_MINUTE` | `5` | 分钟 |
| `XRT_TIME_INTERVAL_SECOND` | `6` | 秒 |
| `XRT_TIME_INTERVAL_WEEKDAY` | `7` | 星期 |
| `XRT_TIME_INTERVAL_QUARTER` | `8` | 季度 |

### 时间格式常量

| 常量 | 值 | 输出格式 |
|------|-----|--------|
| `XRT_TIME_FORMAT_DATETIME` | `0` | `YYYY-MM-DD HH:MM:SS` |
| `XRT_TIME_FORMAT_DATE` | `1` | `YYYY-MM-DD` |
| `XRT_TIME_FORMAT_TIME` | `2` | `HH:MM:SS` |

---

## 时间获取

### xrtNow

获取当前日期和时间。

**函数原型：**
```c
XXAPI xtime xrtNow();
```

**返回值：**
- 当前日期时间的时间戳（秒）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    printf("时间戳: %" PRId64 "\n", now);
    
    // 转换为可读字符串
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDate

获取当前日期（不含时间部分）。

**函数原型：**
```c
XXAPI xtime xrtDate();
```

**返回值：**
- 当前日期的时间戳（时分秒为 00:00:00）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime today = xrtDate();
    str dateStr = xrtTimeToStr(today, XRT_TIME_FORMAT_DATE);
    printf("今天: %s\n", dateStr);
    xrtFree(dateStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTime

获取当前时间（不含日期部分）。

**函数原型：**
```c
XXAPI xtime xrtTime();
```

**返回值：**
- 当日已过的秒数（0-86399）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime currentTime = xrtTime();
    printf("今日已过秒数: %" PRId64 "\n", currentTime);
    
    str timeStr = xrtTimeToStr(currentTime, XRT_TIME_FORMAT_TIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimer

获取高精度计时器（用于性能测量）。

**函数原型：**
```c
XXAPI double xrtTimer();
```

**返回值：**
- 高精度时间（秒，浮点数）

**说明：**
- Windows: 使用 `QueryPerformanceCounter`
- Linux/macOS: 使用 `clock_gettime(CLOCK_MONOTONIC)`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

void HeavyWork() {
    // 模拟耗时操作
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    HeavyWork();
    double end = xrtTimer();
    
    printf("耗时: %.6f 秒\n", end - start);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 返回的是单调递增的时间，适合计算时间间隔
- 不受系统时间调整影响

---

### xrtSleep

毫秒级延时。

**函数原型：**
```c
XXAPI void xrtSleep(uint32 ms);
```

**参数：**
- `ms` - 延时毫秒数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("开始等待...\n");
    xrtSleep(1000);  // 等待 1 秒
    printf("1 秒后\n");
    
    xrtSleep(500);   // 等待 500 毫秒
    printf("又过了 0.5 秒\n");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `Sleep()`
- Linux/macOS 使用 `usleep()`

---

### xrtNowStr / xrtDateStr / xrtTimeStr

获取字符串格式的当前时间。

**函数原型：**
```c
XXAPI str xrtNowStr();   // 返回 "YYYY-MM-DD HH:MM:SS"
XXAPI str xrtDateStr();  // 返回 "YYYY-MM-DD"
XXAPI str xrtTimeStr();  // 返回 "HH:MM:SS"
```

**返回值：**
- 格式化的时间字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str now = xrtNowStr();
    str date = xrtDateStr();
    str time = xrtTimeStr();
    
    printf("当前日期时间: %s\n", now);   // "2025-12-03 15:30:45"
    printf("当前日期: %s\n", date);      // "2025-12-03"
    printf("当前时间: %s\n", time);      // "15:30:45"
    
    xrtFree(now);
    xrtFree(date);
    xrtFree(time);
    
    xrtUnit();
    return 0;
}
```

---

## 时间构建

### xrtDateSerial

构建日期时间戳。

**函数原型：**
```c
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
```

**参数：**
- `iYear` - 年份（支持负数表示公元前）
- `iMonth` - 月份（1-12）
- `iDay` - 日期（1-31）

**返回值：**
- 时间戳（秒）
- 月份无效时返回 0 并设置错误信息

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建日期
    xtime date = xrtDateSerial(2025, 1, 10);
    str dateStr = xrtTimeToStr(date, XRT_TIME_FORMAT_DATE);
    printf("日期: %s\n", dateStr);  // "2025-01-10"
    xrtFree(dateStr);
    
    // 公元前日期（负年份）
    xtime bc = xrtDateSerial(-100, 3, 15);
    printf("公元前时间戳: %" PRId64 "\n", bc);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeSerial

构建时间戳（当日秒数）。

**函数原型：**
```c
XXAPI xtime xrtTimeSerial(int iHour, int iMin, int iSec);
```

**参数：**
- `iHour` - 小时（0-23）
- `iMin` - 分钟（0-59）
- `iSec` - 秒（0-59）

**返回值：**
- 时间戳（当日秒数，0-86399）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建时间
    xtime time = xrtTimeSerial(15, 30, 45);
    printf("当日秒数: %" PRId64 "\n", time);  // 55845 = 15*3600 + 30*60 + 45
    
    str timeStr = xrtTimeToStr(time, XRT_TIME_FORMAT_TIME);
    printf("时间: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateTimeSerial

构建完整的日期+时间戳。

**函数原型：**
```c
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
```

**参数：**
- `iYear` - 年份
- `iMonth` - 月份（1-12）
- `iDay` - 日期（1-31）
- `iHour` - 小时（0-23）
- `iMinute` - 分钟（0-59）
- `iSecond` - 秒（0-59）

**返回值：**
- 完整时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建完整日期时间
    xtime dt = xrtDateTimeSerial(2025, 12, 25, 10, 30, 0);
    str dtStr = xrtTimeToStr(dt, XRT_TIME_FORMAT_DATETIME);
    printf("圣诞节: %s\n", dtStr);  // "2025-12-25 10:30:00"
    xrtFree(dtStr);
    
    // 计算结果等于 xrtDateSerial + xrtTimeSerial
    xtime d = xrtDateSerial(2025, 12, 25);
    xtime t = xrtTimeSerial(10, 30, 0);
    printf("相等: %s\n", (d + t == dt) ? "是" : "否");  // "是"
    
    xrtUnit();
    return 0;
}
```

---

## 时间计算

### xrtDateAdd

时间单位累加。

**函数原型：**
```c
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
```

**参数：**
- `interval` - 间隔类型（使用 `XRT_TIME_INTERVAL_*` 常量）
  - `XRT_TIME_INTERVAL_YEAR` (1) - 年
  - `XRT_TIME_INTERVAL_MONTH` (2) - 月
  - `XRT_TIME_INTERVAL_DAY` (3) - 天
  - `XRT_TIME_INTERVAL_HOUR` (4) - 小时
  - `XRT_TIME_INTERVAL_MINUTE` (5) - 分钟
  - `XRT_TIME_INTERVAL_SECOND` (6) - 秒
  - `XRT_TIME_INTERVAL_WEEKDAY` (7) - 星期（7天）
  - `XRT_TIME_INTERVAL_QUARTER` (8) - 季度（3个月）
- `iValue` - 增量（可为负数）
- `iTime` - 基准时间

**返回值：**
- 计算后的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    str nowStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前: %s\n", nowStr);
    xrtFree(nowStr);
    
    // 加 1 天
    xtime tomorrow = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, now);
    str s1 = xrtTimeToStr(tomorrow, XRT_TIME_FORMAT_DATE);
    printf("明天: %s\n", s1);
    xrtFree(s1);
    
    // 加 1 周
    xtime next_week = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 1, now);
    str s2 = xrtTimeToStr(next_week, XRT_TIME_FORMAT_DATE);
    printf("下周: %s\n", s2);
    xrtFree(s2);
    
    // 减 1 个月
    xtime last_month = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -1, now);
    str s3 = xrtTimeToStr(last_month, XRT_TIME_FORMAT_DATE);
    printf("上月: %s\n", s3);
    xrtFree(s3);
    
    // 加 1 季度
    xtime next_quarter = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 1, now);
    str s4 = xrtTimeToStr(next_quarter, XRT_TIME_FORMAT_DATE);
    printf("下季度: %s\n", s4);
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateDiff

计算时间差。

**函数原型：**
```c
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

**参数：**
- `interval` - 间隔单位（同 `xrtDateAdd`，但**不支持** `XRT_TIME_INTERVAL_WEEKDAY`）
- `iTime1` - 时间1
- `iTime2` - 时间2

**返回值：**
- 时间差（iTime2 - iTime1）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 12, 31);
    
    // 天数差
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, start, end);
    printf("相差天数: %" PRId64 "\n", days);  // 364
    
    // 月数差
    int64 months = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, start, end);
    printf("相差月数: %" PRId64 "\n", months);  // 11
    
    // 季度差
    int64 quarters = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, start, end);
    printf("相差季度: %" PRId64 "\n", quarters);  // 3
    
    // 小时差
    xtime t1 = xrtDateTimeSerial(2025, 1, 1, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 1, 1, 17, 30, 0);
    int64 hours = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, t1, t2);
    printf("相差小时: %" PRId64 "\n", hours);  // 9
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `XRT_TIME_INTERVAL_WEEKDAY` 不支持，返回 0
- 计算结果为 iTime2 - iTime1，可能为负数

---

## 时间格式化

### xrtTimeToStr

时间转字符串

**函数原型：**
```c
XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
```

**参数：**
- `iTime` - 时间戳
- `iFormat` - 格式类型
  - `0` - `YYYY-MM-DD HH:MM:SS`
  - `1` - `YYYY-MM-DD`
  - `2` - `HH:MM:SS`
  - `3` - `YYYYMMDDHHMMSS`

**返回值：**
- 格式化的时间字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xtime now = xrtNow();
str s0 = xrtTimeToStr(now, 0);  // "2025-01-10 15:30:45"
str s1 = xrtTimeToStr(now, 1);  // "2025-01-10"
str s2 = xrtTimeToStr(now, 2);  // "15:30:45"
str s3 = xrtTimeToStr(now, 3);  // "20250110153045"
xrtFree(s0); xrtFree(s1); xrtFree(s2); xrtFree(s3);
```

---

## 时间提取

### xrtYear

获取时间中的年份。

**函数原型：**
```c
XXAPI int64 xrtYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 年份（负数表示公元前）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int64 year = xrtYear(now);
    printf("当前年份: %" PRId64 "\n", year);  // 2025
    
    xrtUnit();
    return 0;
}
```

---

### xrtMonth

获取时间中的月份。

**函数原型：**
```c
XXAPI int xrtMonth(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 月份（1-12）

---

### xrtDay

获取时间中的日期。

**函数原型：**
```c
XXAPI int xrtDay(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 日期（1-31）

---

### xrtHour

获取时间中的小时。

**函数原型：**
```c
XXAPI int xrtHour(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 小时（0-23）

---

### xrtMinute

获取时间中的分钟。

**函数原型：**
```c
XXAPI int xrtMinute(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 分钟（0-59）

---

### xrtSecond

获取时间中的秒。

**函数原型：**
```c
XXAPI int xrtSecond(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 秒（0-59）

---

### xrtWeekday

获取时间的星期。

**函数原型：**
```c
XXAPI int xrtWeekday(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 星期（0=星期日，1=星期一，...，6=星期六）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int weekday = xrtWeekday(now);
    const char* days[] = {"日", "一", "二", "三", "四", "五", "六"};
    printf("今天是星期%s\n", days[weekday]);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDayOfYear

获取时间是当年的第几天。

**函数原型：**
```c
XXAPI int xrtDayOfYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 当年第几天（1-366）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int day_of_year = xrtDayOfYear(now);
    printf("今天是今年第 %d 天\n", day_of_year);
    
    // 1月1日是第1天
    xtime jan1 = xrtDateSerial(2025, 1, 1);
    printf("1月1日: 第 %d 天\n", xrtDayOfYear(jan1));  // 1
    
    xrtUnit();
    return 0;
}
```

**完整时间提取示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    printf("当前时间: %" PRId64 "-", xrtYear(now));
    printf("%02d-%02d ", xrtMonth(now), xrtDay(now));
    printf("%02d:%02d:%02d\n", xrtHour(now), xrtMinute(now), xrtSecond(now));
    printf("星期: %d\n", xrtWeekday(now));
    printf("年内第: %d 天\n", xrtDayOfYear(now));
    
    xrtUnit();
    return 0;
}
```

---

## 时间解码

### xrtDecodeSerial

一次性解码时间戳的所有组件。

**函数原型：**
```c
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, 
                           int* pHour, int* pMinute, int* pSecond, 
                           int* pWeekday, int* pDayOfYear);
```

**参数：**
- `iTime` - 时间戳
- `pYear` - 年份输出指针（可NULL）
- `pMonth` - 月份输出指针（可NULL）
- `pDay` - 日期输出指针（可NULL）
- `pHour` - 小时输出指针（可NULL）
- `pMinute` - 分钟输出指针（可NULL）
- `pSecond` - 秒输出指针（可NULL）
- `pWeekday` - 星期输出指针（可NULL）
- `pDayOfYear` - 年内天数输出指针（可NULL）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // 解码所有字段
    int64 year;
    int month, day, hour, minute, second, weekday, dayOfYear;
    xrtDecodeSerial(now, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);
    
    printf("完整解码:\n");
    printf("日期: %" PRId64 "-%02d-%02d\n", year, month, day);
    printf("时间: %02d:%02d:%02d\n", hour, minute, second);
    printf("星期: %d\n", weekday);
    printf("年内第: %d 天\n", dayOfYear);
    
    // 只解码部分字段
    int64 y;
    int m, d;
    xrtDecodeSerial(now, &y, &m, &d, NULL, NULL, NULL, NULL, NULL);
    printf("仅日期: %" PRId64 "-%02d-%02d\n", y, m, d);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 比多次调用单独的提取函数效率更高
- 不需要的字段可传 `NULL`

---

## 日期工具

### xrtIsLeapYear

判断是否为闰年。

**函数原型：**
```c
XXAPI bool xrtIsLeapYear(int iYear);
```

**参数：**
- `iYear` - 年份

**返回值：**
- `TRUE` - 闰年
- `FALSE` - 平年

**判断规则：**
- 能被 400 整除 → 闰年
- 能被 100 整除 → 平年
- 能被 4 整除 → 闰年
- 其他 → 平年

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %s\n", xrtIsLeapYear(2024) ? "闰年" : "平年");  // 闰年
    printf("2025: %s\n", xrtIsLeapYear(2025) ? "闰年" : "平年");  // 平年
    printf("2000: %s\n", xrtIsLeapYear(2000) ? "闰年" : "平年");  // 闰年
    printf("1900: %s\n", xrtIsLeapYear(1900) ? "闰年" : "平年");  // 平年
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInMonth

获取某年某月有多少天。

**函数原型：**
```c
XXAPI int xrtDaysInMonth(int iYear, int iMonth);
```

**参数：**
- `iYear` - 年份
- `iMonth` - 月份（1-12）

**返回值：**
- 天数（28-31）
- 月份无效返回 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 2月天数取决于是否闰年
    printf("2024年2月: %d 天\n", xrtDaysInMonth(2024, 2));  // 29
    printf("2025年2月: %d 天\n", xrtDaysInMonth(2025, 2));  // 28
    
    // 其他月份
    printf("1月: %d 天\n", xrtDaysInMonth(2025, 1));   // 31
    printf("4月: %d 天\n", xrtDaysInMonth(2025, 4));   // 30
    printf("12月: %d 天\n", xrtDaysInMonth(2025, 12)); // 31
    
    // 无效月份
    printf("无效月份: %d\n", xrtDaysInMonth(2025, 13));  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInYear

获取某年有多少天。

**函数原型：**
```c
XXAPI int xrtDaysInYear(int iYear);
```

**参数：**
- `iYear` - 年份

**返回值：**
- 天数（365 或 366）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024年: %d 天\n", xrtDaysInYear(2024));  // 366
    printf("2025年: %d 天\n", xrtDaysInYear(2025));  // 365
    
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 性能计时器

```c
#include "xrt.h"
#include <stdio.h>

void ProcessData() {
    // 模拟耗时操作
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    ProcessData();
    double elapsed = xrtTimer() - start;
    
    printf("处理耗时: %.6f 秒\n", elapsed);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 日志时间戳

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
    
    Log((str)"应用程序启动");
    xrtSleep(1000);
    Log((str)"执行中...");
    xrtSleep(500);
    Log((str)"应用程序结束");
    
    xrtUnit();
    return 0;
}
```

---

### 3. 过期检查

```c
#include "xrt.h"
#include <stdio.h>

bool IsExpired(xtime expire_time) {
    return xrtNow() > expire_time;
}

int main() {
    xrtInit();
    
    // 设置过期时间为 2025年12月31日
    xtime expire = xrtDateSerial(2025, 12, 31);
    
    if (IsExpired(expire)) {
        printf("许可证已过期\n");
    } else {
        int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, xrtNow(), expire);
        printf("许可证剩余 %" PRId64 " 天\n", days);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. 生日计算

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 假设生日是 1990年5月15日
    xtime birthday = xrtDateSerial(1990, 5, 15);
    xtime today = xrtDate();
    
    // 计算年龄
    int64 years = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, birthday, today);
    printf("年龄: %" PRId64 " 岁\n", years);
    
    // 计算总天数
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, birthday, today);
    printf("已经活了: %" PRId64 " 天\n", days);
    
    // 下一个生日
    int64 this_year = xrtYear(today);
    xtime next_bday = xrtDateSerial(this_year, 5, 15);
    if (next_bday <= today) {
        next_bday = xrtDateSerial(this_year + 1, 5, 15);
    }
    int64 days_until = xrtDateDiff(XRT_TIME_INTERVAL_DAY, today, next_bday);
    printf("距离下次生日: %" PRId64 " 天\n", days_until);
    
    xrtUnit();
    return 0;
}
```

---

### 5. 工作日计算

```c
#include "xrt.h"
#include <stdio.h>

// 计算工作日数（不含周末）
int CountWorkdays(xtime start, xtime end) {
    int workdays = 0;
    xtime current = start;
    
    while (current <= end) {
        int weekday = xrtWeekday(current);
        if (weekday != 0 && weekday != 6) {  // 非周末
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
    printf("2025年1月工作日: %d 天\n", workdays);
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [String 字符串处理](api-string.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#time-时间处理库)

</div>
