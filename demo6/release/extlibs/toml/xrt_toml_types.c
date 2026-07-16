#include "xrt_toml_internal.h"



static int __xrtTomlDateTimeCompare(const ptr pLeft, const ptr pRight)
{
	return memcmp(pLeft, pRight, sizeof(xrttomldatetime));
}

static uint64 __xrtTomlDateTimeHash(const ptr pValue)
{
	return xrtHash64(pValue, sizeof(xrttomldatetime));
}

static str __xrtTomlDateTimeTypeToString(const ptr pValue, uint32* pSize)
{
	str sText = xrtTomlDateTimeToString((const xrttomldatetime*)pValue);
	if ( pSize != NULL ) { *pSize = sText != NULL ? (uint32)strlen((const char*)sText) : 0u; }
	return sText;
}

static const xrt_type_ops __xrtTomlDateTimeOps = {
	.compare = __xrtTomlDateTimeCompare,
	.hash = __xrtTomlDateTimeHash,
	.to_string = __xrtTomlDateTimeTypeToString
};

#define XRT_TOML_TYPE_DESC(symbol, id, name) \
	static const xrt_type_desc symbol = { \
		.TypeId = id, .Kind = XRT_TYPE_KIND_RECORD, \
		.Name = name, .NameSize = (uint32)(sizeof(name) - 1u), \
		.AbiName = "xrttomldatetime", .AbiNameSize = 16u, \
		.Size = sizeof(xrttomldatetime), .Align = _Alignof(xrttomldatetime), \
		.Ops = &__xrtTomlDateTimeOps \
	}

XRT_TOML_TYPE_DESC(__xrtTomlDateType, 0x544f4d11u, "toml.LocalDate");
XRT_TOML_TYPE_DESC(__xrtTomlTimeType, 0x544f4d12u, "toml.LocalTime");
XRT_TOML_TYPE_DESC(__xrtTomlDateTimeType, 0x544f4d13u, "toml.LocalDateTime");
XRT_TOML_TYPE_DESC(__xrtTomlOffsetDateTimeType, 0x544f4d14u, "toml.OffsetDateTime");



XXAPI const xrt_type_desc* xrtTomlDateType(void) { return &__xrtTomlDateType; }
XXAPI const xrt_type_desc* xrtTomlTimeType(void) { return &__xrtTomlTimeType; }
XXAPI const xrt_type_desc* xrtTomlDateTimeType(void) { return &__xrtTomlDateTimeType; }
XXAPI const xrt_type_desc* xrtTomlOffsetDateTimeType(void) { return &__xrtTomlOffsetDateTimeType; }

static const xrt_type_desc* __xrtTomlTypeForKind(int iKind)
{
	switch ( iKind ) {
	case XRT_TOML_DATE: return xrtTomlDateType();
	case XRT_TOML_TIME: return xrtTomlTimeType();
	case XRT_TOML_DATETIME: return xrtTomlDateTimeType();
	case XRT_TOML_OFFSET_DATETIME: return xrtTomlOffsetDateTimeType();
	default: return NULL;
	}
}

static bool __xrtTomlValidDateTime(const xrttomldatetime* pValue)
{
	if ( pValue == NULL || pValue->iKind < XRT_TOML_DATE ||
		pValue->iKind > XRT_TOML_OFFSET_DATETIME ) { return FALSE; }
	if ( pValue->iMicrosecond < 0 || pValue->iMicrosecond > 999999 ) { return FALSE; }
	if ( pValue->iKind != XRT_TOML_TIME &&
		(pValue->iYear < 0 || pValue->iYear > 9999 ||
		 pValue->iMonth < 1 || pValue->iMonth > 12 ||
		 pValue->iDay < 1 || pValue->iDay > xrtDaysInMonth(pValue->iYear, pValue->iMonth)) ) {
		return FALSE;
	}
	if ( pValue->iKind != XRT_TOML_DATE &&
		(pValue->iHour < 0 || pValue->iHour > 23 || pValue->iMinute < 0 ||
		 pValue->iMinute > 59 || pValue->iSecond < 0 || pValue->iSecond > 59) ) {
		return FALSE;
	}
	return pValue->iKind != XRT_TOML_OFFSET_DATETIME ||
		(pValue->iTimezoneMinutes >= -1439 && pValue->iTimezoneMinutes <= 1439);
}

static bool __xrtTomlValidDateParts(int iYear, int iMonth, int iDay)
{
	return iYear >= 0 && iYear <= 9999 && iMonth >= 1 && iMonth <= 12 &&
		iDay >= 1 && iDay <= xrtDaysInMonth(iYear, iMonth);
}

static bool __xrtTomlValidTimeParts(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	return iHour >= 0 && iHour <= 23 && iMinute >= 0 && iMinute <= 59 &&
		iSecond >= 0 && iSecond <= 59 && iMicrosecond >= 0 &&
		iMicrosecond <= 999999;
}

static xvalue __xrtTomlCreateDateTimeValue(const xrttomldatetime* pValue)
{
	const xrt_type_desc* pType = __xrtTomlTypeForKind(pValue != NULL ? pValue->iKind : 0);
	return pType != NULL && __xrtTomlValidDateTime(pValue) ?
		xvoCreateRecord(pType, (ptr)pValue) : NULL;
}

XXAPI xvalue xrtTomlCreateDate(int iYear, int iMonth, int iDay)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ) { return NULL; }
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		0, 0, 0, 0, 0, XRT_TOML_DATE, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XXAPI xvalue xrtTomlCreateTime(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	if ( !__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ) { return NULL; }
	xrttomldatetime tValue = { 0, 0, 0, (int16)iHour, (int16)iMinute,
		(int16)iSecond, iMicrosecond, 0, XRT_TOML_TIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XXAPI xvalue xrtTomlCreateDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ||
		!__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ) { return NULL; }
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		(int16)iHour, (int16)iMinute, (int16)iSecond, iMicrosecond, 0,
		XRT_TOML_DATETIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XXAPI xvalue xrtTomlCreateOffsetDateTime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond,
	int iTimezoneMinutes)
{
	if ( !__xrtTomlValidDateParts(iYear, iMonth, iDay) ||
		!__xrtTomlValidTimeParts(iHour, iMinute, iSecond, iMicrosecond) ||
		iTimezoneMinutes < -1439 || iTimezoneMinutes > 1439 ) { return NULL; }
	xrttomldatetime tValue = { (int16)iYear, (int16)iMonth, (int16)iDay,
		(int16)iHour, (int16)iMinute, (int16)iSecond, iMicrosecond,
		(int16)iTimezoneMinutes, XRT_TOML_OFFSET_DATETIME, 0 };
	return __xrtTomlCreateDateTimeValue(&tValue);
}

XXAPI bool xrtTomlIsDateTimeValue(xvalue pValue, int* pKind)
{
	const xrt_type_desc* pType = xvoTypeDesc(pValue);
	int iKind = 0;
	if ( pType == xrtTomlDateType() ) { iKind = XRT_TOML_DATE; }
	else if ( pType == xrtTomlTimeType() ) { iKind = XRT_TOML_TIME; }
	else if ( pType == xrtTomlDateTimeType() ) { iKind = XRT_TOML_DATETIME; }
	else if ( pType == xrtTomlOffsetDateTimeType() ) { iKind = XRT_TOML_OFFSET_DATETIME; }
	if ( pKind != NULL ) { *pKind = iKind; }
	return iKind != 0;
}

XXAPI const xrttomldatetime* xrtTomlGetDateTime(xvalue pValue)
{
	return xrtTomlIsDateTimeValue(pValue, NULL) ?
		(const xrttomldatetime*)xvoGetRecordData(pValue) : NULL;
}

XXAPI str xrtTomlDateTimeToString(const xrttomldatetime* pValue)
{
	char sText[96];
	char sFraction[16] = "";
	char sZone[16] = "";
	int iSize;
	if ( !__xrtTomlValidDateTime(pValue) ) { return NULL; }
	if ( pValue->iMicrosecond != 0 ) {
		int iEnd;
		snprintf(sFraction, sizeof(sFraction), ".%06d", pValue->iMicrosecond);
		iEnd = (int)strlen(sFraction) - 1;
		while ( iEnd > 0 && sFraction[iEnd] == '0' ) { sFraction[iEnd--] = 0; }
	}
	if ( pValue->iKind == XRT_TOML_OFFSET_DATETIME ) {
		int iOffset = pValue->iTimezoneMinutes;
		if ( iOffset == 0 ) {
			sZone[0] = 'Z';
			sZone[1] = 0;
		}
		else {
			char cSign = iOffset < 0 ? '-' : '+';
			if ( iOffset < 0 ) { iOffset = -iOffset; }
			snprintf(sZone, sizeof(sZone), "%c%02d:%02d", cSign,
				iOffset / 60, iOffset % 60);
		}
	}
	if ( pValue->iKind == XRT_TOML_DATE ) {
		iSize = snprintf(sText, sizeof(sText), "%04d-%02d-%02d",
			pValue->iYear, pValue->iMonth, pValue->iDay);
	} else if ( pValue->iKind == XRT_TOML_TIME ) {
		iSize = snprintf(sText, sizeof(sText), "%02d:%02d:%02d%s",
			pValue->iHour, pValue->iMinute, pValue->iSecond, sFraction);
	} else {
		iSize = snprintf(sText, sizeof(sText), "%04d-%02d-%02dT%02d:%02d:%02d%s%s",
			pValue->iYear, pValue->iMonth, pValue->iDay, pValue->iHour,
			pValue->iMinute, pValue->iSecond, sFraction, sZone);
	}
	return iSize > 0 && (size_t)iSize < sizeof(sText) ? xrtCopyStr((str)sText, (size_t)iSize) : NULL;
}
