


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


