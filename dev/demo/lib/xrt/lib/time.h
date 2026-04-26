


// 获取高精度时钟 Tick ( 返回秒数，便于计算时间间隔 )
XXAPI double xrtTimer()
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( xCore.Frequency == 0.0 ) {
			return (double)GetTickCount64() * 0.001;
		} else {
			LARGE_INTEGER QPC;
			QueryPerformanceCounter(&QPC);
			return (double)QPC.QuadPart / (double)xCore.Frequency;
		}
	#else
		// 其他平台方案
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		return timer.tv_sec + ((double)timer.tv_nsec * 0.000000001);
	#endif
}



// 毫秒级延时
XXAPI void xrtSleep(uint32 ms)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		Sleep(ms);
	#else
		// 其他平台方案
		usleep(ms * 1000);
	#endif
}



// 判断是否为闰年
XXAPI bool xrtIsLeapYear(int iYear)
{
	if ( (iYear % 400) == 0 ) {
		return TRUE;
	} else if ( (iYear % 100) == 0 ) {
		return FALSE;
	} else {
		if ( (iYear & 3) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
}



// 获取某年某月有多少天
XXAPI int xrtDaysInMonth(int iYear, int iMonth)
{
	static const int arrDays[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if ( (iMonth >= 1) && (iMonth <= 12) ) {
		if ( iMonth == 2 ) {
			if ( xrtIsLeapYear(iYear) ) {
				return 29;
			} else {
				return 28;
			}
		} else {
			return arrDays[iMonth - 1];
		}
	} else {
		return 0;
	}
}



// 获取某年有多少天
XXAPI int xrtDaysInYear(int iYear)
{
	if ( xrtIsLeapYear(iYear) ) {
		return 366;
	} else {
		return 365;
	}
}



// 构建时间
XXAPI xtime xrtTimeSerial(int iHour, int iMinute, int iSecond)
{
	return (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
}



// 构建日期
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay)
{
	if ( (iMonth < 1) || (iMonth > 12) ) {
		xrtSetError("Month range error !", FALSE);
		return 0;
	}
	xtime iDate = (iDay - 1) * XRT_TIME_DAY;
	for ( int i = 1; i < iMonth; i++ ) {
		iDate += xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
	}
	if ( iYear < 0 ) {
		// 公元前
		iYear = llabs(iYear);
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( int i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear(i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
		iDate = iDate * -1;
	} else {
		// 公元后
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( int i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear(i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
	}
	return iDate;
}



// 构建日期 + 时间
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xtime iTime = (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
	xtime iDate = xrtDateSerial(iYear, iMonth, iDay);
	return iDate + iTime;
}



// 获取时间中的秒
XXAPI int xrtSecond(xtime iTime)
{
	return iTime % XRT_TIME_MINUTE;
}



// 获取时间中的分钟
XXAPI int xrtMinute(xtime iTime)
{
	return (iTime % XRT_TIME_HOUR) / 60;
}



// 获取时间中的小时
XXAPI int xrtHour(xtime iTime)
{
	return (iTime % XRT_TIME_DAY) / XRT_TIME_HOUR;
}



// 获取时间中的日期
XXAPI int xrtDay(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	int iDay = 1;
	for ( int i = 1; i <= 31; i++ ) {
		if ( iYearMod >= XRT_TIME_DAY ) {
			iYearMod -= XRT_TIME_DAY;
		} else {
			iDay = i;
			break;
		}
	}
	return iDay;
}



// 获取时间中的月份
XXAPI int xrtMonth(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	return iMonth;
}



// 获取时间中的年份
XXAPI int64 xrtYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( iTime < 0 ) {
		return -iYear;
	} else {
		return iYear;
	}
}



// 获取时间中的星期
XXAPI int xrtWeekday(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iDay = iTimeAbs / XRT_TIME_DAY;
	return iDay % 7;
}



// 获取时间是当年的第几天
XXAPI int xrtDayOfYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	return 1 + (iYearMod / XRT_TIME_DAY);
}



// 解码时间
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, int* pHour, int* pMinute, int* pSecond, int* pWeekday, int* pDayOfYear)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( pYear ) {
		if ( iTime < 0 ) {
			*pYear = -iYear;
		} else {
			*pYear = iYear;
		}
	}
	if ( pDayOfYear ) {
		*pDayOfYear = 1 + (iYearMod / XRT_TIME_DAY);
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	if ( pMonth ) {
		*pMonth = iMonth;
	}
	if ( pDay ) {
		int iDay = 1;
		for ( int i = 1; i <= 31; i++ ) {
			if ( iYearMod >= XRT_TIME_DAY ) {
				iYearMod -= XRT_TIME_DAY;
			} else {
				iDay = i;
				break;
			}
		}
		*pDay = iDay;
	}
	if ( pHour ) {
		*pHour = (iTimeAbs % XRT_TIME_DAY) / XRT_TIME_HOUR;
	}
	if ( pMinute ) {
		*pMinute = (iTimeAbs % XRT_TIME_HOUR) / 60;
	}
	if ( pSecond ) {
		*pSecond = iTimeAbs % XRT_TIME_MINUTE;
	}
	if ( pWeekday ) {
		uint64 iDay = iTimeAbs / XRT_TIME_DAY;
		*pWeekday = iDay % 7;
	}
}



// 获取当前日期 + 时间 (线程安全)
XXAPI xtime xrtNow()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtDateTimeSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}



// 获取当前日期 (线程安全)
XXAPI xtime xrtDate()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtDateSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday);
}



// 获取当前时间 (线程安全)
XXAPI xtime xrtTime()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtTimeSerial(stm.tm_hour, stm.tm_min, stm.tm_sec);
}



// 获取字符串格式的当前日期 + 时间（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtNowStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%d-%02d-%02d %02d:%02d:%02d", 1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}



// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtDateStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%d-%02d-%02d", 1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday);
}



// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）(线程安全)
XXAPI str xrtTimeStr()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&stm, &rawtime);
	#else
		localtime_r(&rawtime, &stm);
	#endif
	return xrtFormat("%02d:%02d:%02d", stm.tm_hour, stm.tm_min, stm.tm_sec);
}



// 转换日期 + 时间为字符串（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtTimeToStr(xtime iTime, int iFormat)
{
	if ( iFormat == XRT_TIME_FORMAT_DATETIME ) {
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtFormat("%d-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
	} else if ( iFormat == XRT_TIME_FORMAT_DATE ) {
		int64 iYear;
		int iMonth, iDay;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, NULL, NULL, NULL, NULL, NULL);
		return xrtFormat("%d-%02d-%02d", iYear, iMonth, iDay);
	} else if ( iFormat == XRT_TIME_FORMAT_TIME ) {
		int iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, NULL, NULL, NULL, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtFormat("%02d:%02d:%02d", iHour, iMinute, iSecond);
	} else {
		return xCore.sNull;
	}
}



// 时间单位累加
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime)
{
	if ( interval == XRT_TIME_INTERVAL_YEAR ) {
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		return xrtDateTimeSerial(iYear + iValue, iMonth, iDay, iHour, iMinute, iSecond);
	} else if ( interval == XRT_TIME_INTERVAL_MONTH ) {
		xtime iValueAbs = llabs(iValue);
		uint64 iAddYear = iValueAbs / 12;
		uint64 iAddMonth = iValueAbs % 12;
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		if ( iValue < 0 ) {
			if ( iMonth - iAddMonth < 1 ) {
				iYear = iYear - iAddYear - 1;
				iMonth = 12 - (iAddMonth - iMonth);
			} else {
				iYear = iYear - iAddYear;
				iMonth -= iAddMonth;
			}
		} else {
			if ( iMonth + iAddMonth > 12 ) {
				iYear = iYear + iAddYear + 1;
				iMonth = (iMonth + iAddMonth) % 12;
			} else {
				iYear = iYear + iAddYear;
				iMonth += iAddMonth;
			}
		}
		xtime tRet = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return tRet;
	} else if ( interval == XRT_TIME_INTERVAL_DAY ) {
		return iTime + (iValue * XRT_TIME_DAY);
	} else if ( interval == XRT_TIME_INTERVAL_HOUR ) {
		return iTime + (iValue * XRT_TIME_HOUR);
	} else if ( interval == XRT_TIME_INTERVAL_MINUTE ) {
		return iTime + (iValue * XRT_TIME_MINUTE);
	} else if ( interval == XRT_TIME_INTERVAL_SECOND ) {
		return iTime + iValue;
	} else if ( interval == XRT_TIME_INTERVAL_WEEKDAY ) {
		return iTime + (iValue * XRT_TIME_DAY * 7);
	} else if ( interval == XRT_TIME_INTERVAL_QUARTER ) {
		return xrtDateAdd(XRT_TIME_INTERVAL_MONTH, iValue * 3, iTime);
	} else {
		return iTime;
	}
}



// 单位时间差计算（ 不支持 XRT_TIME_INTERVAL_WEEKDAY ）
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2)
{
	if ( interval == XRT_TIME_INTERVAL_YEAR ) {
		int64 iYear1 = xrtYear(iTime1);
		int64 iYear2 = xrtYear(iTime2);
		return iYear2 - iYear1;
	} else if ( interval == XRT_TIME_INTERVAL_MONTH ) {
		int64 iYear1, iYear2;
		int iMonth1, iMonth2;
		xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
		xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
		return ((iYear2 - iYear1) * 12) + (iMonth2 - iMonth1);
	} else if ( interval == XRT_TIME_INTERVAL_DAY ) {
		return (iTime2 - iTime1) / XRT_TIME_DAY;
	} else if ( interval == XRT_TIME_INTERVAL_HOUR ) {
		return (iTime2 - iTime1) / XRT_TIME_HOUR;
	} else if ( interval == XRT_TIME_INTERVAL_MINUTE ) {
		return (iTime2 - iTime1) / XRT_TIME_MINUTE;
	} else if ( interval == XRT_TIME_INTERVAL_SECOND ) {
		return iTime2 - iTime1;
	} else if ( interval == XRT_TIME_INTERVAL_QUARTER ) {
		int64 iYear1, iYear2;
		int iMonth1, iMonth2;
		xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
		xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
		return (((iYear2 - iYear1) * 12) + (iMonth2 - iMonth1)) / 3;
	} else {
		return 0;
	}
}



// 获取季度（1-4）
XXAPI int xrtQuarter(xtime iTime)
{
	int iMonth = xrtMonth(iTime);
	return ((iMonth - 1) / 3) + 1;
}



// 获取日期部分（去除时间）
XXAPI xtime xrtDatePart(xtime iTime)
{
	xtime iMod = iTime % XRT_TIME_DAY;
	if ( iMod < 0 ) {
		iMod += XRT_TIME_DAY;
	}
	return iTime - iMod;
}



// 获取时间部分（去除日期）
XXAPI xtime xrtTimePart(xtime iTime)
{
	xtime iMod = iTime % XRT_TIME_DAY;
	if ( iMod < 0 ) {
		iMod += XRT_TIME_DAY;
	}
	return iMod;
}



// 是否同一天
XXAPI bool xrtIsSameDay(xtime iTime1, xtime iTime2)
{
	return xrtDatePart(iTime1) == xrtDatePart(iTime2);
}



// 是否同一月
XXAPI bool xrtIsSameMonth(xtime iTime1, xtime iTime2)
{
	int64 iYear1, iYear2;
	int iMonth1, iMonth2;
	xrtDecodeSerial(iTime1, &iYear1, &iMonth1, NULL, NULL, NULL, NULL, NULL, NULL);
	xrtDecodeSerial(iTime2, &iYear2, &iMonth2, NULL, NULL, NULL, NULL, NULL, NULL);
	return (iYear1 == iYear2) && (iMonth1 == iMonth2);
}



// 是否同一年
XXAPI bool xrtIsSameYear(xtime iTime1, xtime iTime2)
{
	return xrtYear(iTime1) == xrtYear(iTime2);
}



// 判断时间是否在区间内
XXAPI bool xrtTimeInRange(xtime iTime, xtime iStart, xtime iEnd)
{
	return (iTime >= iStart) && (iTime <= iEnd);
}



// 判断两个时间区间是否重叠
XXAPI bool xrtTimeRangeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2)
{
	return (iStart1 <= iEnd2) && (iEnd1 >= iStart2);
}



// 与Unix时间戳互转 - xtime转Unix时间戳
XXAPI int64 xrtToUnixTime(xtime iTime)
{
	return iTime - XRT_TIME_19700101;
}



// 与Unix时间戳互转 - Unix时间戳转xtime
XXAPI xtime xrtFromUnixTime(int64 unixTime)
{
	return XRT_TIME_19700101 + unixTime;
}



// 获取月份的第一天
XXAPI xtime xrtFirstDayOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth;
	xrtDecodeSerial(iTime, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	return xrtDateSerial(iYear, iMonth, 1);
}



// 获取月份的最后一天
XXAPI xtime xrtLastDayOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth;
	xrtDecodeSerial(iTime, &iYear, &iMonth, NULL, NULL, NULL, NULL, NULL, NULL);
	int iDays = xrtDaysInMonth((int)iYear, iMonth);
	return xrtDateSerial(iYear, iMonth, iDays);
}



// 获取年份的第一天
XXAPI xtime xrtFirstDayOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	return xrtDateSerial(iYear, 1, 1);
}



// 获取年份的最后一天
XXAPI xtime xrtLastDayOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	return xrtDateSerial(iYear, 12, 31);
}



// 获取周的第一天（iStartDay: 0=周日, 1=周一, ...）
XXAPI xtime xrtFirstDayOfWeek(xtime iTime, int iStartDay)
{
	int iWeekday = xrtWeekday(iTime);
	int iDiff = iWeekday - iStartDay;
	if ( iDiff < 0 ) {
		iDiff += 7;
	}
	return xrtDatePart(iTime) - (iDiff * XRT_TIME_DAY);
}



// 获取周的最后一天（iStartDay: 0=周日, 1=周一, ...）
XXAPI xtime xrtLastDayOfWeek(xtime iTime, int iStartDay)
{
	return xrtFirstDayOfWeek(iTime, iStartDay) + (6 * XRT_TIME_DAY);
}



// 获取当年第几周（ISO周数，周一为一周开始）
XXAPI int xrtWeekOfYear(xtime iTime)
{
	int64 iYear = xrtYear(iTime);
	xtime iFirstDayOfYear = xrtDateSerial(iYear, 1, 1);
	int iFirstWeekday = xrtWeekday(iFirstDayOfYear);
	int iFirstDayOffset = (iFirstWeekday == 0) ? 6 : (iFirstWeekday - 1);
	int iDayOfYear = xrtDayOfYear(iTime);
	return ((iDayOfYear - 1 + iFirstDayOffset) / 7) + 1;
}



// 获取当月第几周（周一为一周开始）
XXAPI int xrtWeekOfMonth(xtime iTime)
{
	int64 iYear;
	int iMonth, iDay;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, NULL, NULL, NULL, NULL, NULL);
	xtime iFirstDayOfMonth = xrtDateSerial(iYear, iMonth, 1);
	int iFirstWeekday = xrtWeekday(iFirstDayOfMonth);
	int iFirstDayOffset = (iFirstWeekday == 0) ? 6 : (iFirstWeekday - 1);
	return ((iDay - 1 + iFirstDayOffset) / 7) + 1;
}



// 获取UTC时间
XXAPI xtime xrtNowUTC()
{
	time_t rawtime = time(NULL);
	struct tm stm;
	#if defined(_WIN32) || defined(_WIN64)
		#ifdef __TINYC__
			// TCC 不支持 gmtime_s，使用 gmtime
			struct tm* pstm = gmtime(&rawtime);
			if ( pstm ) {
				stm = *pstm;
			} else {
				return 0;
			}
		#else
			if ( gmtime_s(&stm, &rawtime) != 0 ) {
				return 0;
			}
		#endif
	#else
		gmtime_r(&rawtime, &stm);
	#endif
	return xrtDateTimeSerial(1900 + stm.tm_year, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
}



// 获取本地时区偏移（秒）
XXAPI int xrtTimezoneOffset()
{
	time_t rawtime = time(NULL);
	struct tm stmLocal, stmUTC;
	#if defined(_WIN32) || defined(_WIN64)
		#ifdef __TINYC__
			// TCC 不支持 localtime_s/gmtime_s，使用普通版本
			struct tm* pLocal = localtime(&rawtime);
			if ( pLocal ) { stmLocal = *pLocal; } else { return 0; }
			struct tm* pUTC = gmtime(&rawtime);
			if ( pUTC ) { stmUTC = *pUTC; } else { return 0; }
		#else
			localtime_s(&stmLocal, &rawtime);
			if ( gmtime_s(&stmUTC, &rawtime) != 0 ) {
				return 0;
			}
		#endif
	#else
		localtime_r(&rawtime, &stmLocal);
		gmtime_r(&rawtime, &stmUTC);
	#endif
	xtime iLocal = xrtDateTimeSerial(1900 + stmLocal.tm_year, stmLocal.tm_mon + 1, stmLocal.tm_mday, stmLocal.tm_hour, stmLocal.tm_min, stmLocal.tm_sec);
	xtime iUTC = xrtDateTimeSerial(1900 + stmUTC.tm_year, stmUTC.tm_mon + 1, stmUTC.tm_mday, stmUTC.tm_hour, stmUTC.tm_min, stmUTC.tm_sec);
	return (int)(iLocal - iUTC);
}



// UTC转本地时间
XXAPI xtime xrtUTCToLocal(xtime utc)
{
	return utc + xrtTimezoneOffset();
}



// 本地时间转UTC
XXAPI xtime xrtLocalToUTC(xtime local)
{
	return local - xrtTimezoneOffset();
}



// 获取相对时间描述（如"3天前"、"2小时后"）
XXAPI str xrtRelativeTime(xtime iTime, xtime iBaseTime)
{
	int64 iDiff = iTime - iBaseTime;
	bool bFuture = (iDiff >= 0);
	int64 iAbsDiff = llabs(iDiff);
	const char* sSuffix = bFuture ? "后" : "前";
	if ( iAbsDiff < 60 ) {
		return xrtFormat("%lld秒%s", iAbsDiff, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_HOUR ) {
		return xrtFormat("%lld分钟%s", iAbsDiff / 60, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY ) {
		return xrtFormat("%lld小时%s", iAbsDiff / XRT_TIME_HOUR, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY * 30 ) {
		return xrtFormat("%lld天%s", iAbsDiff / XRT_TIME_DAY, sSuffix);
	} else if ( iAbsDiff < XRT_TIME_DAY * 365 ) {
		return xrtFormat("%lld个月%s", iAbsDiff / (XRT_TIME_DAY * 30), sSuffix);
	} else {
		return xrtFormat("%lld年%s", iAbsDiff / (XRT_TIME_DAY * 365), sSuffix);
	}
}



// 字符串转时间（智能解析，支持多种格式）
XXAPI xtime xrtStrToTime(str sTime, size_t iSize)
{
	if ( !sTime ) return 0;
	if ( iSize == 0 ) iSize = strlen(sTime);
	if ( iSize == 0 ) return 0;
	
	int year = 0, month = 1, day = 1, hour = 0, minute = 0, second = 0;
	const char* p = sTime;
	const char* end = sTime + iSize;
	
	// 跳过前导非数字
	while ( p < end && (*p < '0' || *p > '9') ) p++;
	if ( p >= end ) return 0;
	
	// 解析第一个数字序列
	int64 num1 = 0;
	int len1 = 0;
	while ( p < end && *p >= '0' && *p <= '9' ) {
		num1 = num1 * 10 + (*p - '0');
		len1++; p++;
	}
	
	if ( len1 == 8 ) {
		// YYYYMMDD
		year = (int)(num1 / 10000);
		month = (int)((num1 / 100) % 100);
		day = (int)(num1 % 100);
	} else if ( len1 == 14 ) {
		// YYYYMMDDHHMMSS
		year = (int)(num1 / 10000000000LL);
		month = (int)((num1 / 100000000LL) % 100);
		day = (int)((num1 / 1000000LL) % 100);
		hour = (int)((num1 / 10000LL) % 100);
		minute = (int)((num1 / 100LL) % 100);
		second = (int)(num1 % 100);
	} else if ( len1 == 4 ) {
		year = num1;
		// 跳过分隔符
		while ( p < end && (*p < '0' || *p > '9') ) p++;
		if ( p < end ) {
			int num2 = 0;
			while ( p < end && *p >= '0' && *p <= '9' ) {
				num2 = num2 * 10 + (*p - '0'); p++;
			}
			month = num2;
			while ( p < end && (*p < '0' || *p > '9') ) p++;
			if ( p < end ) {
				int num3 = 0;
				while ( p < end && *p >= '0' && *p <= '9' ) {
					num3 = num3 * 10 + (*p - '0'); p++;
				}
				day = num3;
				// 继续解析时间
				while ( p < end && (*p < '0' || *p > '9') ) p++;
				if ( p < end ) {
					int num4 = 0;
					while ( p < end && *p >= '0' && *p <= '9' ) {
						num4 = num4 * 10 + (*p - '0'); p++;
					}
					hour = num4;
					while ( p < end && (*p < '0' || *p > '9') ) p++;
					if ( p < end ) {
						int num5 = 0;
						while ( p < end && *p >= '0' && *p <= '9' ) {
							num5 = num5 * 10 + (*p - '0'); p++;
						}
						minute = num5;
						while ( p < end && (*p < '0' || *p > '9') ) p++;
						if ( p < end ) {
							int num6 = 0;
							while ( p < end && *p >= '0' && *p <= '9' ) {
								num6 = num6 * 10 + (*p - '0'); p++;
							}
							second = num6;
						}
					}
				}
			}
		}
	} else if ( len1 <= 2 ) {
		// 可能是 HH:MM:SS 或 MM-DD
		char sep = (p < end) ? *p : 0;
		if ( sep == ':' ) {
			hour = num1;
			p++;
			if ( p < end ) {
				int num2 = 0;
				while ( p < end && *p >= '0' && *p <= '9' ) {
					num2 = num2 * 10 + (*p - '0'); p++;
				}
				minute = num2;
				if ( p < end && *p == ':' ) {
					p++;
					int num3 = 0;
					while ( p < end && *p >= '0' && *p <= '9' ) {
						num3 = num3 * 10 + (*p - '0'); p++;
					}
					second = num3;
				}
			}
			year = 0; month = 1; day = 1;
		}
	}
	
	return xrtDateTimeSerial(year, month, day, hour, minute, second);
}



// ============================================================================
// 时间格式化与解析 (xrtTimeFormat / xrtTimeParse)
// ============================================================================

// 英文月份表
static const char* _xrtMonthShort[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* _xrtMonthFull[]  = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

// 英文星期表
static const char* _xrtWeekShort[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* _xrtWeekFull[]  = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// 格式单元类型
enum _XRT_FMT_TYPE {
	_XRT_FMT_LITERAL = 0,   // 固定文本
	_XRT_FMT_YEAR4,         // yyyy
	_XRT_FMT_YEAR2,         // yy
	_XRT_FMT_MONTH2,        // mm (月份，补0)
	_XRT_FMT_MONTH1,        // m  (月份，不补0)
	_XRT_FMT_MONTH_SHORT,   // mmm (Jan)
	_XRT_FMT_MONTH_FULL,    // mmmm (January)
	_XRT_FMT_DAY2,          // dd
	_XRT_FMT_DAY1,          // d
	_XRT_FMT_HOUR24_2,      // hh (24小时制)
	_XRT_FMT_HOUR24_1,      // h
	_XRT_FMT_HOUR12_2,      // HH (12小时制)
	_XRT_FMT_HOUR12_1,      // H
	_XRT_FMT_MINUTE2,       // nn 或 h后的mm
	_XRT_FMT_MINUTE1,       // n
	_XRT_FMT_SECOND2,       // ss
	_XRT_FMT_SECOND1,       // s
	_XRT_FMT_AMPM_LOWER,    // ap
	_XRT_FMT_AMPM_UPPER,    // AP
	_XRT_FMT_WEEKDAY_NUM,   // w
	_XRT_FMT_WEEKDAY_SHORT, // ww
	_XRT_FMT_WEEKDAY_FULL,  // www
	_XRT_FMT_QUARTER,       // q
	_XRT_FMT_SKIP_ANY,      // * (跳过任意非数字)
	_XRT_FMT_SKIP_ONE_PLUS, // . (跳过至少1个非数字)
	_XRT_FMT_SKIP_CHAR,     // ? (跳过1个字符)
	_XRT_FMT_SKIP_SPACE     // 空格 (跳过空白)
};

// 格式单元结构
typedef struct {
	int type;             // 单元类型
	char text[64];        // 固定文本内容
	int textLen;          // 文本长度
} _XrtFmtCell;

// 格式解析结果
typedef struct {
	_XrtFmtCell cells[64]; // 最多64个单元
	int count;             // 单元数量
} _XrtFmtResult;

// 辅助：比较字符（不区分大小写）
static inline int _xrtCharLower(int c) {
	return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

// 辅助：不区分大小写比较字符串
static inline int _xrtStrNCmpI(const char* s1, const char* s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		int c1 = _xrtCharLower((unsigned char)s1[i]);
		int c2 = _xrtCharLower((unsigned char)s2[i]);
		if (c1 != c2) return c1 - c2;
		if (c1 == 0) return 0;
	}
	return 0;
}

// 辅助：写入2位数字（补前导0）
static inline void _xrtWrite2Digit(char* buf, int val) {
	buf[0] = '0' + (val / 10);
	buf[1] = '0' + (val % 10);
}

// 辅助：解析数字（返回解析位数，val存储结果）
static inline int _xrtParseDigits(const char* s, const char* end, int minLen, int maxLen, int* val) {
	if (!s || s >= end) return 0;
	int len = 0;
	int v = 0;
	while (s + len < end && len < maxLen && s[len] >= '0' && s[len] <= '9') {
		v = v * 10 + (s[len] - '0');
		len++;
	}
	if (len < minLen) return 0;
	if (len == maxLen && s + len < end && s[len] >= '0' && s[len] <= '9') {
		return 0;
	}
	*val = v;
	return len;
}

// 辅助：匹配英文月份（返回月份1-12，0表示失败）
static inline int _xrtMatchMonth(const char* s, const char* end, int* consumed) {
	if (!s || s >= end) return 0;
	size_t remain = end - s;
	for (int i = 0; i < 12; i++) {
		size_t len = strlen(_xrtMonthFull[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtMonthFull[i], len) == 0) {
			*consumed = (int)len;
			return i + 1;
		}
	}
	for (int i = 0; i < 12; i++) {
		size_t len = strlen(_xrtMonthShort[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtMonthShort[i], len) == 0) {
			*consumed = (int)len;
			return i + 1;
		}
	}
	return 0;
}

// 辅助：匹配英文星期（返回星期0-6，-1表示失败）
static inline int _xrtMatchWeekday(const char* s, const char* end, int* consumed) {
	if (!s || s >= end) return -1;
	size_t remain = end - s;
	for (int i = 0; i < 7; i++) {
		size_t len = strlen(_xrtWeekFull[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtWeekFull[i], len) == 0) {
			*consumed = (int)len;
			return i;
		}
	}
	for (int i = 0; i < 7; i++) {
		size_t len = strlen(_xrtWeekShort[i]);
		if (remain >= len && _xrtStrNCmpI(s, _xrtWeekShort[i], len) == 0) {
			*consumed = (int)len;
			return i;
		}
	}
	return -1;
}

// 解析格式字符串
static void _xrtParseFormat(const char* fmt, _XrtFmtResult* result, int forParse) {
	result->count = 0;
	if (!fmt) return;
	if (forParse) {
		result->cells[result->count].type = _XRT_FMT_SKIP_ANY;
		result->cells[result->count].textLen = 0;
		result->count++;
	}
	const char* p = fmt;
	int afterHour = 0;
	while (*p && result->count < 63) {
		_XrtFmtCell* cell = &result->cells[result->count];
		cell->textLen = 0;
		if (p[0] == 'y') {
			int cnt = 0;
			while (p[cnt] == 'y' && cnt < 4) cnt++;
			if (cnt >= 4) { cell->type = _XRT_FMT_YEAR4; p += 4; }
			else { cell->type = _XRT_FMT_YEAR2; p += cnt; }
			afterHour = 0;
			result->count++;
		} else if (p[0] == 'm') {
			int cnt = 0;
			while (p[cnt] == 'm' && cnt < 4) cnt++;
			if (cnt >= 4) { cell->type = _XRT_FMT_MONTH_FULL; p += 4; afterHour = 0; }
			else if (cnt == 3) { cell->type = _XRT_FMT_MONTH_SHORT; p += 3; afterHour = 0; }
			else if (cnt == 2) { cell->type = afterHour ? _XRT_FMT_MINUTE2 : _XRT_FMT_MONTH2; p += 2; }
			else { cell->type = _XRT_FMT_MONTH1; p += 1; afterHour = 0; }
			result->count++;
		} else if (p[0] == 'd') {
			int cnt = 0;
			while (p[cnt] == 'd' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_DAY2 : _XRT_FMT_DAY1;
			p += cnt; afterHour = 0;
			result->count++;
		} else if (p[0] == 'h') {
			int cnt = 0;
			while (p[cnt] == 'h' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_HOUR24_2 : _XRT_FMT_HOUR24_1;
			p += cnt; afterHour = 1;
			result->count++;
		} else if (p[0] == 'H') {
			int cnt = 0;
			while (p[cnt] == 'H' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_HOUR12_2 : _XRT_FMT_HOUR12_1;
			p += cnt; afterHour = 1;
			result->count++;
		} else if (p[0] == 'n') {
			int cnt = 0;
			while (p[cnt] == 'n' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_MINUTE2 : _XRT_FMT_MINUTE1;
			p += cnt;
			result->count++;
		} else if (p[0] == 's') {
			int cnt = 0;
			while (p[cnt] == 's' && cnt < 2) cnt++;
			cell->type = (cnt >= 2) ? _XRT_FMT_SECOND2 : _XRT_FMT_SECOND1;
			p += cnt;
			result->count++;
		} else if (p[0] == 'a' && p[1] == 'p') {
			cell->type = _XRT_FMT_AMPM_LOWER; p += 2;
			result->count++;
		} else if (p[0] == 'A' && p[1] == 'P') {
			cell->type = _XRT_FMT_AMPM_UPPER; p += 2;
			result->count++;
		} else if (p[0] == 'w') {
			int cnt = 0;
			while (p[cnt] == 'w' && cnt < 3) cnt++;
			if (cnt >= 3) { cell->type = _XRT_FMT_WEEKDAY_FULL; p += 3; }
			else if (cnt == 2) { cell->type = _XRT_FMT_WEEKDAY_SHORT; p += 2; }
			else { cell->type = _XRT_FMT_WEEKDAY_NUM; p += 1; }
			result->count++;
		} else if (p[0] == 'q') {
			cell->type = _XRT_FMT_QUARTER; p += 1;
			result->count++;
		} else if (forParse && p[0] == '*') {
			cell->type = _XRT_FMT_SKIP_ANY; p += 1;
			result->count++;
		} else if (forParse && p[0] == '.') {
			cell->type = _XRT_FMT_SKIP_ONE_PLUS; p += 1;
			result->count++;
		} else if (forParse && p[0] == '?') {
			cell->type = _XRT_FMT_SKIP_CHAR; p += 1;
			result->count++;
		} else if (forParse && (p[0] == ' ' || p[0] == '\t')) {
			cell->type = _XRT_FMT_SKIP_SPACE;
			while (*p == ' ' || *p == '\t') p++;
			result->count++;
		} else {
			cell->type = _XRT_FMT_LITERAL;
			int len = 0;
			while (p[len] && len < 63) {
				char c = p[len];
				if (c == 'y' || c == 'm' || c == 'd' || c == 'h' || c == 'H' ||
				    c == 'n' || c == 's' || c == 'w' || c == 'q' ||
				    (c == 'a' && p[len+1] == 'p') || (c == 'A' && p[len+1] == 'P')) break;
				if (forParse && (c == '*' || c == '.' || c == '?' || c == ' ' || c == '\t')) break;
				cell->text[len] = c; len++;
			}
			if (len > 0) {
				cell->text[len] = '\0'; cell->textLen = len; p += len;
				result->count++;
			} else { p++; }
		}
	}
}

// 时间格式化为字符串
XXAPI str xrtTimeFormat(xtime iTime, str sFormat)
{
	if (!sFormat) return NULL;
	_XrtFmtResult fmt;
	_xrtParseFormat(sFormat, &fmt, 0);
	if (fmt.count == 0) return NULL;
	int64 iYear; int iMonth, iDay, iHour, iMinute, iSecond, iWeekday;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, NULL);
	int iQuarter = ((iMonth - 1) / 3) + 1;
	int iHour12 = iHour % 12; if (iHour12 == 0) iHour12 = 12;
	int isPM = (iHour >= 12);
	size_t bufSize = 256;
	char* buf = (char*)xrtMalloc(bufSize);
	if (!buf) return NULL;
	size_t pos = 0;
	for (int i = 0; i < fmt.count && pos < bufSize - 32; i++) {
		_XrtFmtCell* cell = &fmt.cells[i];
		switch (cell->type) {
			case _XRT_FMT_LITERAL:
				if (pos + cell->textLen < bufSize) { memcpy(buf + pos, cell->text, cell->textLen); pos += cell->textLen; }
				break;
			case _XRT_FMT_YEAR4: pos += snprintf(buf + pos, bufSize - pos, "%04lld", iYear); break;
			case _XRT_FMT_YEAR2: pos += snprintf(buf + pos, bufSize - pos, "%02d", (int)(iYear % 100)); break;
			case _XRT_FMT_MONTH2: _xrtWrite2Digit(buf + pos, iMonth); pos += 2; break;
			case _XRT_FMT_MONTH1: pos += snprintf(buf + pos, bufSize - pos, "%d", iMonth); break;
			case _XRT_FMT_MONTH_SHORT:
				if (iMonth >= 1 && iMonth <= 12) { size_t len = strlen(_xrtMonthShort[iMonth - 1]); memcpy(buf + pos, _xrtMonthShort[iMonth - 1], len); pos += len; }
				break;
			case _XRT_FMT_MONTH_FULL:
				if (iMonth >= 1 && iMonth <= 12) { size_t len = strlen(_xrtMonthFull[iMonth - 1]); memcpy(buf + pos, _xrtMonthFull[iMonth - 1], len); pos += len; }
				break;
			case _XRT_FMT_DAY2: _xrtWrite2Digit(buf + pos, iDay); pos += 2; break;
			case _XRT_FMT_DAY1: pos += snprintf(buf + pos, bufSize - pos, "%d", iDay); break;
			case _XRT_FMT_HOUR24_2: _xrtWrite2Digit(buf + pos, iHour); pos += 2; break;
			case _XRT_FMT_HOUR24_1: pos += snprintf(buf + pos, bufSize - pos, "%d", iHour); break;
			case _XRT_FMT_HOUR12_2: _xrtWrite2Digit(buf + pos, iHour12); pos += 2; break;
			case _XRT_FMT_HOUR12_1: pos += snprintf(buf + pos, bufSize - pos, "%d", iHour12); break;
			case _XRT_FMT_MINUTE2: _xrtWrite2Digit(buf + pos, iMinute); pos += 2; break;
			case _XRT_FMT_MINUTE1: pos += snprintf(buf + pos, bufSize - pos, "%d", iMinute); break;
			case _XRT_FMT_SECOND2: _xrtWrite2Digit(buf + pos, iSecond); pos += 2; break;
			case _XRT_FMT_SECOND1: pos += snprintf(buf + pos, bufSize - pos, "%d", iSecond); break;
			case _XRT_FMT_AMPM_LOWER: buf[pos++] = isPM ? 'p' : 'a'; buf[pos++] = 'm'; break;
			case _XRT_FMT_AMPM_UPPER: buf[pos++] = isPM ? 'P' : 'A'; buf[pos++] = 'M'; break;
			case _XRT_FMT_WEEKDAY_NUM: buf[pos++] = '0' + iWeekday; break;
			case _XRT_FMT_WEEKDAY_SHORT:
				if (iWeekday >= 0 && iWeekday <= 6) { size_t len = strlen(_xrtWeekShort[iWeekday]); memcpy(buf + pos, _xrtWeekShort[iWeekday], len); pos += len; }
				break;
			case _XRT_FMT_WEEKDAY_FULL:
				if (iWeekday >= 0 && iWeekday <= 6) { size_t len = strlen(_xrtWeekFull[iWeekday]); memcpy(buf + pos, _xrtWeekFull[iWeekday], len); pos += len; }
				break;
			case _XRT_FMT_QUARTER: buf[pos++] = '0' + iQuarter; break;
			default: break;
		}
	}
	buf[pos] = '\0';
	return buf;
}

// 字符串解析为时间
XXAPI xtime xrtTimeParse(str sTime, str sFormat)
{
	if (!sTime || !sFormat) return 0;
	_XrtFmtResult fmt;
	_xrtParseFormat(sFormat, &fmt, 1);
	if (fmt.count == 0) return 0;
	const char* s = sTime;
	const char* end = sTime + strlen(sTime);
	int year = 0, month = 1, day = 1, hour = 0, minute = 0, second = 0;
	int isPM = -1, is12Hour = 0;
	for (int i = 0; i < fmt.count && s < end; i++) {
		_XrtFmtCell* cell = &fmt.cells[i];
		int val, consumed;
		switch (cell->type) {
			case _XRT_FMT_LITERAL:
				if ((size_t)(end - s) >= (size_t)cell->textLen && memcmp(s, cell->text, cell->textLen) == 0) s += cell->textLen;
				else return 0;
				break;
			case _XRT_FMT_YEAR4:
				consumed = _xrtParseDigits(s, end, 4, 4, &val);
				if (consumed > 0) { year = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_YEAR2:
				consumed = _xrtParseDigits(s, end, 2, 2, &val);
				if (consumed > 0) { year = (val < 70) ? (2000 + val) : (1900 + val); s += consumed; } else return 0;
				break;
			case _XRT_FMT_MONTH2: case _XRT_FMT_MONTH1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 12) { month = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_MONTH_SHORT: case _XRT_FMT_MONTH_FULL:
				val = _xrtMatchMonth(s, end, &consumed);
				if (val > 0) { month = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_DAY2: case _XRT_FMT_DAY1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 31) { day = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_HOUR24_2: case _XRT_FMT_HOUR24_1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 23) { hour = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_HOUR12_2: case _XRT_FMT_HOUR12_1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 1 && val <= 12) { hour = val; is12Hour = 1; s += consumed; } else return 0;
				break;
			case _XRT_FMT_MINUTE2: case _XRT_FMT_MINUTE1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 59) { minute = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_SECOND2: case _XRT_FMT_SECOND1:
				consumed = _xrtParseDigits(s, end, 1, 2, &val);
				if (consumed > 0 && val >= 0 && val <= 59) { second = val; s += consumed; } else return 0;
				break;
			case _XRT_FMT_AMPM_LOWER: case _XRT_FMT_AMPM_UPPER:
				if (end - s >= 2) {
					char c0 = _xrtCharLower(s[0]), c1 = _xrtCharLower(s[1]);
					if (c0 == 'a' && c1 == 'm') { isPM = 0; s += 2; }
					else if (c0 == 'p' && c1 == 'm') { isPM = 1; s += 2; }
					else return 0;
				} else return 0;
				break;
			case _XRT_FMT_WEEKDAY_NUM:
				consumed = _xrtParseDigits(s, end, 1, 1, &val);
				if (consumed > 0 && val >= 0 && val <= 6) s += consumed; else return 0;
				break;
			case _XRT_FMT_WEEKDAY_SHORT: case _XRT_FMT_WEEKDAY_FULL:
				val = _xrtMatchWeekday(s, end, &consumed);
				if (val >= 0) s += consumed; else return 0;
				break;
			case _XRT_FMT_QUARTER:
				consumed = _xrtParseDigits(s, end, 1, 1, &val);
				if (consumed > 0 && val >= 1 && val <= 4) s += consumed; else return 0;
				break;
			case _XRT_FMT_SKIP_ANY:
				// 智能跳过：根据下一个单元类型决定如何跳过
				if (i + 1 < fmt.count) {
					_XrtFmtCell* next = &fmt.cells[i + 1];
					if (next->type == _XRT_FMT_LITERAL) {
						// 下一个是字面量，搜索字面量位置
						const char* found = NULL;
						for (const char* p = s; p + next->textLen <= end; p++) {
							if (memcmp(p, next->text, next->textLen) == 0) {
								found = p; break;
							}
						}
						if (found) s = found;
					} else if (next->type == _XRT_FMT_MONTH_SHORT || next->type == _XRT_FMT_MONTH_FULL) {
						// 下一个是英文月份，搜索月份位置
						while (s < end) {
							if (_xrtMatchMonth(s, end, &consumed) > 0) break;
							s++;
						}
					} else if (next->type == _XRT_FMT_WEEKDAY_SHORT || next->type == _XRT_FMT_WEEKDAY_FULL) {
						// 下一个是英文星期，搜索星期位置
						while (s < end) {
							if (_xrtMatchWeekday(s, end, &consumed) >= 0) break;
							s++;
						}
					} else if (next->type == _XRT_FMT_AMPM_LOWER || next->type == _XRT_FMT_AMPM_UPPER) {
						// 下一个是AM/PM，搜索位置
						while (s + 1 < end) {
							char c0 = _xrtCharLower(s[0]), c1 = _xrtCharLower(s[1]);
							if ((c0 == 'a' || c0 == 'p') && c1 == 'm') break;
							s++;
						}
					} else {
						// 默认：跳过非数字
						while (s < end && (*s < '0' || *s > '9')) s++;
					}
				} else {
					while (s < end && (*s < '0' || *s > '9')) s++;
				}
				break;
			case _XRT_FMT_SKIP_ONE_PLUS:
				if (s < end && (*s < '0' || *s > '9')) { s++; while (s < end && (*s < '0' || *s > '9')) s++; }
				else return 0;
				break;
			case _XRT_FMT_SKIP_CHAR:
				if (s < end) { if ((unsigned char)*s >= 0x80) s += (s + 1 < end) ? 2 : 1; else s++; }
				else return 0;
				break;
			case _XRT_FMT_SKIP_SPACE:
				while (s < end && (*s == ' ' || *s == '\t')) s++;
				break;
			default: break;
		}
	}
	if (is12Hour && isPM >= 0) {
		if (isPM == 1 && hour != 12) hour += 12;
		else if (isPM == 0 && hour == 12) hour = 0;
	}
	if (year == 0) { xtime now = xrtNow(); year = (int)xrtYear(now); }
	return xrtDateTimeSerial(year, month, day, hour, minute, second);
}



// 时间约等于
XXAPI bool xrtTimeApprox(xtime a, xtime b)
{
	if ( a == b ) { return TRUE; }
	
	xtime diff = (a > b) ? (a - b) : (b - a);
	return (diff <= xCore.iApproxTimeTol);
}

