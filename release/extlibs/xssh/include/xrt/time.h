#ifndef XRT_TIME_H
#define XRT_TIME_H

#include <xrt/core.h>



#if (defined(XRT_FEATURE_TIME_LOCAL) || defined(XRT_FEATURE_TIME_TEXT)) && !defined(XRT_FEATURE_TIME)
	#error "XRT local time and time text features require XRT_FEATURE_TIME"
#endif



/* xtime 和固定时长统一使用微秒，避免浮点计时和隐式单位换算。 */
#define XRT_TIME_MICROSECOND	INT64_C(1)
#define XRT_TIME_MILLISECOND	INT64_C(1000)
#define XRT_TIME_SECOND		INT64_C(1000000)
#define XRT_TIME_MINUTE		INT64_C(60000000)
#define XRT_TIME_HOUR		INT64_C(3600000000)
#define XRT_TIME_DAY			INT64_C(86400000000)
#define XRT_TIME_WEEK		INT64_C(604800000000)



/* 星期值固定从星期日零开始，便于和 C/POSIX 及 HTTP-date 对接。 */
typedef enum xtimeweekday {
	XTIME_SUNDAY = 0,
	XTIME_MONDAY,
	XTIME_TUESDAY,
	XTIME_WEDNESDAY,
	XTIME_THURSDAY,
	XTIME_FRIDAY,
	XTIME_SATURDAY
} xtimeweekday;



/* 日期计算单位；月、季度和年使用日历语义，其余单位使用固定时长。 */
typedef enum xtimeunit {
	XTIME_UNIT_MICROSECOND = 0,
	XTIME_UNIT_MILLISECOND,
	XTIME_UNIT_SECOND,
	XTIME_UNIT_MINUTE,
	XTIME_UNIT_HOUR,
	XTIME_UNIT_DAY,
	XTIME_UNIT_WEEK,
	XTIME_UNIT_MONTH,
	XTIME_UNIT_QUARTER,
	XTIME_UNIT_YEAR
} xtimeunit;



/* 本地时间在夏令时回拨区间出现两个候选值时的选择规则。 */
typedef enum xtimefold {
	XTIME_FOLD_REJECT = 0,
	XTIME_FOLD_EARLIER,
	XTIME_FOLD_LATER
} xtimefold;



/* 时间模块稳定错误代码。 */
typedef enum xtimeerror {
	XTIME_ERROR_RANGE = 1,
	XTIME_ERROR_OVERFLOW,
	XTIME_ERROR_FORMAT,
	XTIME_ERROR_PARSE,
	XTIME_ERROR_LOCAL_GAP,
	XTIME_ERROR_LOCAL_FOLD,
	XTIME_ERROR_LOCAL_UNSUPPORTED
} xtimeerror;



/* 分解后的 Gregorian 日期时间；Offset 为 UTC 以东秒数。 */
typedef struct xdatetime {
	int64 Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;
	int Microsecond;
	int Offset;
	int Weekday;
	int YearDay;
	int IsDST;
} xdatetime;



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_TIME)

/* 返回单调递增时钟的微秒计数，只能用于测量间隔和截止时间。 */
XRT_API uint64 xrtClock(void);



/* 返回单调时钟的浮点秒数，供短小的性能测量代码使用。 */
XRT_API double xrtTimer(void);



/* 返回当前 Unix Epoch 微秒。 */
XRT_API xtime xrtNow(void);



/* 至少睡眠指定毫秒；零表示让出当前执行时间片。 */
XRT_API void xrtSleep(uint32 iMilliseconds);



/* 至少睡眠指定微秒。 */
XRT_API void xrtSleepUs(uint64 iMicroseconds);



/* 睡眠到单调时钟截止点；截止点已到时立即返回。 */
XRT_API void xrtSleepUntil(uint64 iDeadline);



/* 判断 Gregorian 年份是否为闰年，支持负年份和零年。 */
XRT_API bool xrtIsLeapYear(int64 iYear);



/* 返回指定月份的天数；月份无效时返回零并设置参数错误。 */
XRT_API int xrtDaysInMonth(int64 iYear, int iMonth);



/* 返回指定年份的天数。 */
XRT_API int xrtDaysInYear(int64 iYear);



/* 构造 UTC 零点日期。 */
XRT_API bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime);



/* 构造 UTC 日期时间。 */
XRT_API bool xrtDateTime(int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond, xtime* pTime);



/* 按结构中的显式 UTC 偏移构造绝对时间。 */
XRT_API bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime);



/* 把绝对时间按 UTC 分解为日期时间。 */
XRT_API bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime);



/* 把绝对时间按固定 UTC 偏移分解，偏移范围为 -23:59:59 到 +23:59:59。 */
XRT_API bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime);



/* 从 Unix 秒安全构造 xtime。 */
XRT_API bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime);



/* 从 Unix 毫秒安全构造 xtime。 */
XRT_API bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime);



/* 返回向负无穷取整的 Unix 秒。 */
XRT_API int64 xrtTimeUnix(xtime iTime);



/* 返回向负无穷取整的 Unix 毫秒。 */
XRT_API int64 xrtTimeUnixMs(xtime iTime);



/* 提取 UTC 年份。 */
XRT_API int64 xrtYear(xtime iTime);



/* 提取 UTC 月份。 */
XRT_API int xrtMonth(xtime iTime);



/* 提取 UTC 月内日期。 */
XRT_API int xrtDay(xtime iTime);



/* 提取 UTC 小时。 */
XRT_API int xrtHour(xtime iTime);



/* 提取 UTC 分钟。 */
XRT_API int xrtMinute(xtime iTime);



/* 提取 UTC 秒。 */
XRT_API int xrtSecond(xtime iTime);



/* 提取秒内微秒。 */
XRT_API int xrtMicrosecond(xtime iTime);



/* 提取星期，范围为 XTIME_SUNDAY 到 XTIME_SATURDAY。 */
XRT_API int xrtWeekday(xtime iTime);



/* 提取年内日期，范围为 1 到 366。 */
XRT_API int xrtDayOfYear(xtime iTime);



/* 提取季度，范围为 1 到 4。 */
XRT_API int xrtQuarter(xtime iTime);



/* 返回 UTC 当日零点。 */
XRT_API xtime xrtDatePart(xtime iTime);



/* 返回 UTC 当日已经经过的微秒，范围为 [0, XRT_TIME_DAY)。 */
XRT_API xtime xrtTimePart(xtime iTime);



/* 使用显式微秒容差比较两个时间，计算覆盖完整 int64 域。 */
XRT_API bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance);



/* 判断两个 UTC 时间是否位于同一个 Gregorian 日期。 */
XRT_API bool xrtTimeSameDay(xtime iLeft, xtime iRight);



/* 判断两个 UTC 时间是否位于同一个 Gregorian 月份。 */
XRT_API bool xrtTimeSameMonth(xtime iLeft, xtime iRight);



/* 判断两个 UTC 时间是否位于同一个 Gregorian 年份。 */
XRT_API bool xrtTimeSameYear(xtime iLeft, xtime iRight);



/* 判断时间是否位于闭区间；反向区间返回 false。 */
XRT_API bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd);



/* 判断两个闭区间是否重叠；任一反向区间返回 false。 */
XRT_API bool xrtTimeOverlap(xtime iStart1, xtime iEnd1,
	xtime iStart2, xtime iEnd2);



/* 增加固定时长或 Gregorian 日历单位，月末会钳制到目标月最后一天。 */
XRT_API bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult);



/* 计算从起点到终点经过的完整单位数量。 */
XRT_API bool xrtTimeDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult);



/* 返回包含给定时间的半开月份区间 [start, end)。 */
XRT_API bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd);



/* 返回包含给定时间的半开年份区间 [start, end)。 */
XRT_API bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd);



/* 返回包含给定时间的半开星期区间 [start, end)。 */
XRT_API bool xrtWeekRange(xtime iTime, int iFirstWeekday, xtime* pStart, xtime* pEnd);



/* 返回 ISO 8601 周年、周数和星期值，其中星期一为 1，星期日为 7。 */
XRT_API bool xrtISOWeek(xtime iTime, int64* pWeekYear, int* pWeek, int* pWeekday);

#endif



#if defined(XRT_FEATURE_TIME_LOCAL)

/* 使用操作系统当前时区规则分解绝对时间。 */
XRT_API bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime);



/* 使用操作系统时区规则构造本地时间，并显式处理 DST 重复区间。 */
XRT_API bool xrtTimeFromLocal(const xdatetime* pDateTime, xtimefold Fold, xtime* pTime);

#endif



#if defined(XRT_FEATURE_TIME_TEXT)

/* 按 % 占位符写入并返回所需字节数；%-m 等数字占位符取消填充，输出缓冲不得与 Format 重叠。 */
XRT_API size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity,
	const xdatetime* pDateTime, xstrview Format);



/* 按 % 占位符创建时间文本；%p 输出大写午别，%P 输出小写午别，返回值由 xrtFree 释放。 */
XRT_API str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format);



/* 严格解析完整格式；%-m 等字段接受一到两位数字，格式非法与文本不匹配使用不同错误码。 */
XRT_API bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime);



/* 按固定 UTC 偏移和上述占位符写入；输出缓冲不得与 Format 重叠。 */
XRT_API size_t xrtTimeWrite(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset, xstrview Format);



/* 按固定 UTC 偏移和上述占位符创建时间文本，返回值由 xrtFree 释放。 */
XRT_API str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format);



/* 严格按上述完整格式解析绝对时间。 */
XRT_API bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime);



/* 写入 RFC 3339 文本；零偏移使用 Z，微秒末尾的零会被删除。 */
XRT_API size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset);



/* 创建 RFC 3339 文本，返回值由 xrtFree 释放。 */
XRT_API str xrtTimeRFC3339(xtime iTime, int iOffset);



/* 严格解析 RFC 3339；超过微秒精度的尾数会向零截断。 */
XRT_API bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime);



/* 写入 HTTP IMF-fixdate，时间始终转换为 GMT 并丢弃秒以下部分。 */
XRT_API size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime);



/* 创建 HTTP IMF-fixdate，返回值由 xrtFree 释放。 */
XRT_API str xrtTimeHTTPDate(xtime iTime);



/* 解析 IMF-fixdate、RFC 850 和 ANSI C asctime 三种 HTTP 日期格式。 */
XRT_API bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime);



/* 尝试解析三种 HTTP 日期格式；失败不修改输出和线程错误。 */
XRT_API bool xrtTimeTryParseHTTPDate(xstrview Text, xtime* pTime);



/* 解析 RFC 3339、HTTP-date 和常见数字日期时间。 */
XRT_API bool xrtTimeParseAny(xstrview Text, xtime* pTime);

#endif



XRT_EXTERN_C_END

#endif
