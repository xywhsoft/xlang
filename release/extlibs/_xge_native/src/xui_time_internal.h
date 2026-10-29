#ifndef XUI_TIME_INTERNAL_H
#define XUI_TIME_INTERNAL_H

#include "xui_internal.h"

#include <string.h>



/* 使用当前 XRT 接口构造 UTC 日期，失败时保留 XRT 错误并返回零。 */
static inline xtime __xuiDateValue(int64 iYear, int iMonth, int iDay)
{
	xtime tValue = 0;

	(void)xrtDate(iYear, iMonth, iDay, &tValue);
	return tValue;
}



/* 使用当前 XRT 接口构造 UTC 日期时间。 */
static inline bool __xuiDateTimeValue(int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, xtime* pValue)
{
	return xrtDateTime(iYear, iMonth, iDay,
		iHour, iMinute, iSecond, 0, pValue);
}



/* 将时间分解为 XUI 仍在使用的可选字段参数。 */
static inline bool __xuiTimeDecode(xtime tValue,
	int64* pYear, int* pMonth, int* pDay,
	int* pHour, int* pMinute, int* pSecond,
	int* pWeekday, int* pYearDay)
{
	xdatetime DateTime;

	memset(&DateTime, 0, sizeof(DateTime));
	if ( !xrtTimeSplit(tValue, &DateTime) ) return false;
	if ( pYear != NULL ) *pYear = DateTime.Year;
	if ( pMonth != NULL ) *pMonth = DateTime.Month;
	if ( pDay != NULL ) *pDay = DateTime.Day;
	if ( pHour != NULL ) *pHour = DateTime.Hour;
	if ( pMinute != NULL ) *pMinute = DateTime.Minute;
	if ( pSecond != NULL ) *pSecond = DateTime.Second;
	if ( pWeekday != NULL ) *pWeekday = DateTime.Weekday;
	if ( pYearDay != NULL ) *pYearDay = DateTime.YearDay;
	return true;
}



/* 返回给定时间所在月份的第一天。 */
static inline xtime __xuiMonthFirst(xtime tValue)
{
	xtime tFirst = tValue;

	(void)xrtMonthRange(tValue, &tFirst, NULL);
	return tFirst;
}



/* 按日历单位增加时间，失败时保留原值。 */
static inline xtime __xuiTimeAddValue(xtime tValue, int64 iAmount, xtimeunit Unit)
{
	xtime tResult = tValue;

	(void)xrtTimeAdd(tValue, iAmount, Unit, &tResult);
	return tResult;
}



/* 把 XUI 的日期格式翻译为 XRT 格式，不分配临时内存。 */
static inline bool __xuiTimeTranslateFormat(const char* sFormat,
	char* sOutput, size_t iCapacity)
{
	const char* sToken;
	size_t iTokenSize;
	size_t iInput;
	size_t iOutput;

	if ( (sFormat == NULL) || (sOutput == NULL) || (iCapacity == 0u) ) return false;
	iInput = 0u;
	iOutput = 0u;
	while ( sFormat[iInput] != '\0' ) {
		sToken = NULL;
		iTokenSize = 0u;
		if ( strncmp(sFormat + iInput, "yyyy", 4u) == 0 ) {
			sToken = "%Y";
			iInput += 4u;
		} else if ( strncmp(sFormat + iInput, "yy", 2u) == 0 ) {
			sToken = "%y";
			iInput += 2u;
		} else if ( strncmp(sFormat + iInput, "mm", 2u) == 0 ) {
			sToken = "%m";
			iInput += 2u;
		} else if ( strncmp(sFormat + iInput, "dd", 2u) == 0 ) {
			sToken = "%d";
			iInput += 2u;
		} else if ( strncmp(sFormat + iInput, "hh", 2u) == 0 ) {
			sToken = "%H";
			iInput += 2u;
		} else if ( strncmp(sFormat + iInput, "nn", 2u) == 0 ) {
			sToken = "%M";
			iInput += 2u;
		} else if ( strncmp(sFormat + iInput, "ss", 2u) == 0 ) {
			sToken = "%S";
			iInput += 2u;
		} else if ( sFormat[iInput] == '%' ) {
			sToken = "%%";
			iInput++;
		}

		if ( sToken != NULL ) {
			iTokenSize = 2u;
			if ( (iOutput + iTokenSize) >= iCapacity ) return false;
			memcpy(sOutput + iOutput, sToken, iTokenSize);
			iOutput += iTokenSize;
		} else {
			if ( (iOutput + 1u) >= iCapacity ) return false;
			sOutput[iOutput++] = sFormat[iInput++];
		}
	}
	sOutput[iOutput] = '\0';
	return true;
}



/* 按 XUI 格式严格解析完整文本。 */
static inline bool __xuiTimeParseFormat(const char* sText,
	const char* sFormat, xtime* pValue)
{
	char sXrtFormat[128];
	xstrview Text;
	xstrview Format;

	if ( (sText == NULL) || (sFormat == NULL) || (pValue == NULL) ) return false;
	if ( !__xuiTimeTranslateFormat(sFormat,
		sXrtFormat, sizeof(sXrtFormat)) ) return false;
	Text.Data = sText;
	Text.Size = strlen(sText);
	Format.Data = sXrtFormat;
	Format.Size = strlen(sXrtFormat);
	return xrtTimeParse(Text, Format, pValue);
}

#endif
