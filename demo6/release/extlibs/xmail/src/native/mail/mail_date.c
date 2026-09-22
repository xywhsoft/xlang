#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_DATE)

/* 规范输出始终使用带星期、秒和数字时区的现代格式。 */
static xstrview __xrtMailDateFormat(void)
{
	static const char sFormat[] = "%a, %d %b %Y %H:%M:%S %z";

	return __xrtMailView(sFormat, sizeof(sFormat) - 1u);
}



/* 根据可选星期和秒选择唯一解析格式，避免探测失败污染错误状态。 */
static xstrview __xrtMailDateParseFormat(bool bWeekday, bool bSecond)
{
	if ( bWeekday ) {
		static const char sWithSecond[] = "%a, %-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%a, %-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
	{
		static const char sWithSecond[] = "%-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
}



/* 去掉日期字段外侧的线性空白。 */
static xstrview __xrtMailDateTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 校验邮件日期可以无损表达的年份和时区。 */
static bool __xrtMailDateRange(const xdatetime* pDateTime)
{
	if ( (pDateTime->Year < 1900) || (pDateTime->Year > 9999) ||
		 ((pDateTime->Offset % 60) != 0) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_HEADER,
			"mail date is outside the RFC 5322 range"
		);
		return false;
	}
	return true;
}



/* 按规范形式写入邮件日期。 */
XRT_API bool xrtMailDateWrite(
	xtime iTime,
	int iOffset,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xdatetime DateTime;
	size_t iRequired;

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtTimeSplitAt(iTime, iOffset, &DateTime) ||
		 !__xrtMailDateRange(&DateTime) ) {
		return false;
	}
	iRequired = xrtTimeWrite(NULL, 0, iTime, iOffset, __xrtMailDateFormat());
	if ( iRequired == XRT_NPOS ) {
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtTimeWrite(
		sOutput,
		iCapacity,
		iTime,
		iOffset,
		__xrtMailDateFormat()
	) != iRequired ) {
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范邮件日期。 */
XRT_API str xrtMailDate(xtime iTime, int iOffset, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDateWrite(iTime, iOffset, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDateWrite(
		iTime,
		iOffset,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 解析现代 RFC 5322 日期并保留原时区偏移。 */
XRT_API bool xrtMailDateParse(
	xstrview Text,
	xtime* pTime,
	int* pOffset
)
{
	xstrview Trimmed;
	xdatetime DateTime;
	xtime iTime;
	bool bWeekday;
	size_t iColonCount = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pTime, sizeof(*pTime)) ||
		 !xrtMemRangeValid(pOffset, pOffset != NULL ? sizeof(*pOffset) : 0) ||
		 xrtMemRangesOverlap(pTime, sizeof(*pTime), Text.Data, Text.Size) ||
		 ((pOffset != NULL) &&
		  (xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		   xrtMemRangesOverlap(pTime, sizeof(*pTime), pOffset, sizeof(*pOffset)))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Trimmed = __xrtMailDateTrim(Text);
	if ( Trimmed.Size == 0 ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, "mail date is empty");
		return false;
	}
	for ( size_t i = 0; i < Trimmed.Size; i++ ) {
		if ( Trimmed.Data[i] == ':' ) {
			iColonCount++;
		} else if ( (Trimmed.Data[i] == '\r') || (Trimmed.Data[i] == '\n') ||
			 (Trimmed.Data[i] == 0) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_HEADER,
				"mail date contains an invalid byte"
			);
			return false;
		}
	}
	bWeekday = (Trimmed.Size > 3u) && (Trimmed.Data[3] == ',');
	if ( ((iColonCount != 1u) && (iColonCount != 2u)) ||
		 !xrtDateTimeParse(
			Trimmed,
			__xrtMailDateParseFormat(bWeekday, iColonCount == 2u),
			&DateTime
		) || !__xrtMailDateRange(&DateTime) ||
		 !xrtTimeMake(&DateTime, &iTime) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"mail date does not match RFC 5322"
		);
		return false;
	}
	*pTime = iTime;
	if ( pOffset != NULL ) {
		*pOffset = DateTime.Offset;
	}
	return true;
}

#endif
